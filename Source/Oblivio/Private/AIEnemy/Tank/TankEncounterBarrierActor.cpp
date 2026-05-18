#include "AIEnemy/Tank/TankEncounterBarrierActor.h"

#include "AIEnemy/EnemyBase.h"
#include "AIEnemy/TankEnemy.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

#if WITH_EDITOR
#include "UObject/UnrealType.h"
#endif

void ATankEncounterBarrierActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyEncounterBarrierSizing();
}

#if WITH_EDITOR
void ATankEncounterBarrierActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	ApplyEncounterBarrierSizing();
}
#endif

void ATankEncounterBarrierActor::RefreshWallVisualFitToInnerBlockingVolume()
{
	ApplyEncounterBarrierSizing();
}

void ATankEncounterBarrierActor::ApplyEncounterBarrierSizing()
{
	if (bAutoFitBlockingVolumeToWallMeshBounds)
	{
		FitBlockingVolumeToOptionalWallMeshInner();
	}
	else if (bAutoFitWallVisualToInnerBox)
	{
		FitWallVisualScaleToBlockingVolumeInner();
	}
}

void ATankEncounterBarrierActor::FitBlockingVolumeToOptionalWallMeshInner()
{
	if (!BlockingVolume || !OptionalWallVisual)
	{
		return;
	}

	UStaticMesh const* Mesh = OptionalWallVisual->GetStaticMesh();
	if (!Mesh)
	{
		return;
	}

	FBoxSphereBounds const SMB = Mesh->GetBounds();
	FBox const MeshAABB(SMB.Origin - SMB.BoxExtent, SMB.Origin + SMB.BoxExtent);

	FBox BarrierAABB = MeshAABB.TransformBy(OptionalWallVisual->GetRelativeTransform());
	FVector const CenterShift = BarrierAABB.GetCenter();
	OptionalWallVisual->SetRelativeLocation(OptionalWallVisual->GetRelativeLocation() - CenterShift);

	BarrierAABB = MeshAABB.TransformBy(OptionalWallVisual->GetRelativeTransform());
	FVector const HalfExtents = BarrierAABB.GetExtent();

	FVector const BVSc = BlockingVolume->GetRelativeScale3D().GetAbs();
	BlockingVolume->SetBoxExtent(FVector(
		HalfExtents.X / FMath::Max(BVSc.X, KINDA_SMALL_NUMBER),
		HalfExtents.Y / FMath::Max(BVSc.Y, KINDA_SMALL_NUMBER),
		HalfExtents.Z / FMath::Max(BVSc.Z, KINDA_SMALL_NUMBER)));

	if (EncounterTrigger)
	{
		FVector Expanded = HalfExtents + EncounterTriggerHalfExtentBump;
		FVector const TrSc = EncounterTrigger->GetRelativeScale3D().GetAbs();
		EncounterTrigger->SetBoxExtent(FVector(
			Expanded.X / FMath::Max(TrSc.X, KINDA_SMALL_NUMBER),
			Expanded.Y / FMath::Max(TrSc.Y, KINDA_SMALL_NUMBER),
			Expanded.Z / FMath::Max(TrSc.Z, KINDA_SMALL_NUMBER)));
	}
}

void ATankEncounterBarrierActor::FitWallVisualScaleToBlockingVolumeInner()
{
	if (!bAutoFitWallVisualToInnerBox || !BlockingVolume || !OptionalWallVisual)
	{
		return;
	}

	UStaticMesh const* Mesh = OptionalWallVisual->GetStaticMesh();
	if (!Mesh)
	{
		return;
	}

	const FVector TargetHalfExtent = BlockingVolume->GetScaledBoxExtent();
	const FBoxSphereBounds MeshBounds = Mesh->GetBounds();
	const FVector MeshHalfExtent = MeshBounds.BoxExtent;

	auto SafeRatio = [](float Target, float MeshExtent)
	{
		return MeshExtent > KINDA_SMALL_NUMBER ? Target / MeshExtent : 1.f;
	};

	const FVector Scale(SafeRatio(TargetHalfExtent.X, MeshHalfExtent.X),
		SafeRatio(TargetHalfExtent.Y, MeshHalfExtent.Y),
		SafeRatio(TargetHalfExtent.Z, MeshHalfExtent.Z));

	OptionalWallVisual->SetRelativeScale3D(Scale);

	if (bFitWallVisualZeroRelativeRotation)
	{
		OptionalWallVisual->SetRelativeRotation(FRotator::ZeroRotator);
		OptionalWallVisual->SetRelativeLocation(FVector::ZeroVector);
	}
	else
	{
		const FRotator VisualRotRel = OptionalWallVisual->GetRelativeRotation();
		const FVector MeshBoundsCenterScaled = FVector(MeshBounds.Origin.X * Scale.X,
			MeshBounds.Origin.Y * Scale.Y,
			MeshBounds.Origin.Z * Scale.Z);

		if (!MeshBoundsCenterScaled.IsNearlyZero(KINDA_SMALL_NUMBER))
		{
			OptionalWallVisual->SetRelativeLocation(VisualRotRel.RotateVector(MeshBoundsCenterScaled));
		}
		else
		{
			OptionalWallVisual->SetRelativeLocation(FVector::ZeroVector);
		}
	}
}

ATankEncounterBarrierActor::ATankEncounterBarrierActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);

	BlockingVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("BlockingVolume"));
	SetRootComponent(BlockingVolume);
	BlockingVolume->SetBoxExtent(FVector(80.f, 320.f, 120.f));
	BlockingVolume->SetCollisionProfileName(TEXT("InvisibleWall"));
	BlockingVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BlockingVolume->SetGenerateOverlapEvents(false);
	BlockingVolume->SetHiddenInGame(true);
	BlockingVolume->CanCharacterStepUpOn = ECB_No;

	EncounterTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("EncounterTrigger"));
	EncounterTrigger->SetupAttachment(BlockingVolume);
	EncounterTrigger->SetBoxExtent(FVector(90.f, 340.f, 130.f));
	EncounterTrigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EncounterTrigger->SetCollisionObjectType(ECC_WorldDynamic);
	EncounterTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	EncounterTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	EncounterTrigger->SetGenerateOverlapEvents(true);
	EncounterTrigger->SetHiddenInGame(true);

	OptionalWallVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OptionalWallVisual"));
	OptionalWallVisual->SetupAttachment(BlockingVolume);
	OptionalWallVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	OptionalWallVisual->SetCastShadow(false);
	OptionalWallVisual->SetVisibility(false);
	OptionalWallVisual->SetHiddenInGame(true);
}

void ATankEncounterBarrierActor::BeginPlay()
{
	Super::BeginPlay();

	ApplyEncounterBarrierSizing();

	BindTankDelegatesIfPossible();

	if (bStartEncounterWhenPlayerOverlapsTrigger && EncounterTrigger)
	{
		EncounterTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		EncounterTrigger->OnComponentBeginOverlap.AddDynamic(this, &ATankEncounterBarrierActor::OnEncounterTriggerOverlapBegin);
	}

	PollBarrierFromTank_ServerAndPresentation_AllClients();

	if (UWorld* const World = GetWorld())
	{
		const float Period = FMath::Clamp(BarrierStatePollSeconds, 0.02f, 1.0f);
		World->GetTimerManager().SetTimer(BarrierPollTimerHandle, this,
			&ATankEncounterBarrierActor::PollBarrierFromTank_ServerAndPresentation_AllClients, Period, true, 0.0f);
	}

	RefreshPresentationFromDerivedState();
}

void ATankEncounterBarrierActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BarrierPollTimerHandle);
	}
	if (EncounterTrigger)
	{
		EncounterTrigger->OnComponentBeginOverlap.RemoveDynamic(this,
			&ATankEncounterBarrierActor::OnEncounterTriggerOverlapBegin);
	}
	UnbindTankDelegatesIfPossible();
	Super::EndPlay(EndPlayReason);
}

void ATankEncounterBarrierActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATankEncounterBarrierActor, bTankEncounterBegun);
}

void ATankEncounterBarrierActor::NotifyTankEncounterStarted_Server()
{
	if (!HasAuthority())
	{
		return;
	}
	MarkEncounterBegun_Server();
}

bool ATankEncounterBarrierActor::IsBarrierBlockingPlayers() const
{
	if (!bTankEncounterBegun || !IsValid(WatchedTank))
	{
		return false;
	}
	if (!WatchedTank->IsAlive())
	{
		return false;
	}
	if (bBypassBarrierWhileTankPlacentaDefense && WatchedTank->IsTankPlacentaDefenseActiveForAnim())
	{
		return false;
	}
	return true;
}

void ATankEncounterBarrierActor::OnRep_TankEncounterBegun()
{
	RefreshPresentationFromDerivedState();
}

void ATankEncounterBarrierActor::BindTankDelegatesIfPossible()
{
	if (!IsValid(WatchedTank))
	{
		return;
	}
	WatchedTank->OnEnemyDied.RemoveDynamic(this, &ATankEncounterBarrierActor::HandleBoundTankDied);
	WatchedTank->OnEnemyDied.AddDynamic(this, &ATankEncounterBarrierActor::HandleBoundTankDied);
	WatchedTank->OnEnemyTargetChanged.RemoveDynamic(this, &ATankEncounterBarrierActor::HandleBoundTankTargetChanged);
	WatchedTank->OnEnemyTargetChanged.AddDynamic(this, &ATankEncounterBarrierActor::HandleBoundTankTargetChanged);
	WatchedTank->OnEnemyFSMStateChanged.RemoveDynamic(this, &ATankEncounterBarrierActor::HandleBoundTankFsmStateChanged);
	WatchedTank->OnEnemyFSMStateChanged.AddDynamic(this, &ATankEncounterBarrierActor::HandleBoundTankFsmStateChanged);
}

void ATankEncounterBarrierActor::UnbindTankDelegatesIfPossible()
{
	if (!IsValid(WatchedTank))
	{
		return;
	}
	WatchedTank->OnEnemyDied.RemoveDynamic(this, &ATankEncounterBarrierActor::HandleBoundTankDied);
	WatchedTank->OnEnemyTargetChanged.RemoveDynamic(this, &ATankEncounterBarrierActor::HandleBoundTankTargetChanged);
	WatchedTank->OnEnemyFSMStateChanged.RemoveDynamic(this, &ATankEncounterBarrierActor::HandleBoundTankFsmStateChanged);
}

void ATankEncounterBarrierActor::HandleBoundTankDied(AEnemyBase* Enemy)
{
	if (Enemy != WatchedTank)
	{
		return;
	}
	DestroyBarrierIfAuthorized();
}

void ATankEncounterBarrierActor::HandleBoundTankTargetChanged(AEnemyBase* Enemy, AActor* NewTarget)
{
	if (Enemy != WatchedTank)
	{
		return;
	}

	if (HasAuthority() && bStartEncounterWhenTankAggro && IsValid(NewTarget))
	{
		TryMarkEncounterBegunFromAggro_Server();
	}

	RefreshPresentationFromDerivedState();
}

void ATankEncounterBarrierActor::HandleBoundTankFsmStateChanged(AEnemyBase* Enemy, EEnemyAIState OldState,
	EEnemyAIState NewState)
{
	if (Enemy != WatchedTank)
	{
		return;
	}

	const bool bEncounterCombatState =
		NewState == EEnemyAIState::Chase ||
		NewState == EEnemyAIState::Attack ||
		NewState == EEnemyAIState::Heartbeat ||
		NewState == EEnemyAIState::JumpAttack ||
		NewState == EEnemyAIState::Membrane ||
		NewState == EEnemyAIState::PlacentaDefense;

	if (HasAuthority() && bStartEncounterWhenTankAggro && bEncounterCombatState)
	{
		MarkEncounterBegun_Server();
		return;
	}

	RefreshPresentationFromDerivedState();
}

void ATankEncounterBarrierActor::OnEncounterTriggerOverlapBegin(UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!HasAuthority() || !OtherActor || !OtherActor->IsA(APawn::StaticClass()))
	{
		return;
	}
	MarkEncounterBegun_Server();
}

void ATankEncounterBarrierActor::PollBarrierFromTank_ServerAndPresentation_AllClients()
{
	if (!IsValid(WatchedTank))
	{
		return;
	}

	if (HasAuthority())
	{
		if (!WatchedTank->IsAlive())
		{
			DestroyBarrierIfAuthorized();
			return;
		}
		TryMarkEncounterBegunFromAggro_Server();
	}

	RefreshPresentationFromDerivedState();
}

void ATankEncounterBarrierActor::TryMarkEncounterBegunFromAggro_Server()
{
	if (!HasAuthority() || bTankEncounterBegun || !bStartEncounterWhenTankAggro || !IsValid(WatchedTank))
	{
		return;
	}
	AEnemyBase* const Enemy = WatchedTank;
	if (!IsValid(Enemy->GetTargetActor()))
	{
		return;
	}
	// LOS 는 HasValid 에 포함(탱커 등); 점프 궤적·시야 일시 차단 때문에 착지에서야 true 가 되면 벽 표시도 늦어진다.
	if (Enemy->IsEncounterAggroGateSatisfiedForBarrier())
	{
		MarkEncounterBegun_Server();
	}
}

void ATankEncounterBarrierActor::RefreshPresentationFromDerivedState()
{
	const bool bShouldBlockPlayers = IsBarrierBlockingPlayers();

	if (BlockingVolume)
	{
		if (bShouldBlockPlayers != bLastAppliedBlockingCollision)
		{
			bLastAppliedBlockingCollision = bShouldBlockPlayers;
			if (bShouldBlockPlayers)
			{
				BlockingVolume->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				BlockingVolume->SetCollisionProfileName(TEXT("InvisibleWall"));
			}
			else
			{
				BlockingVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}
		}
	}

	if (OptionalWallVisual)
	{
		const bool bHasMesh = OptionalWallVisual->GetStaticMesh() != nullptr;
		const bool bShow = bShouldBlockPlayers && bHasMesh;
		if (bShow != bLastAppliedVisualVisible)
		{
			bLastAppliedVisualVisible = bShow;
			OptionalWallVisual->SetVisibility(bShow);
			OptionalWallVisual->SetHiddenInGame(!bShow);
		}
	}
}

void ATankEncounterBarrierActor::MarkEncounterBegun_Server()
{
	if (!HasAuthority())
	{
		return;
	}
	if (bTankEncounterBegun)
	{
		return;
	}
	bTankEncounterBegun = true;
	RefreshPresentationFromDerivedState();
}

void ATankEncounterBarrierActor::DestroyBarrierIfAuthorized()
{
	if (!HasAuthority() || bDestroyBarrierQueued)
	{
		return;
	}
	bDestroyBarrierQueued = true;
	Destroy();
}
