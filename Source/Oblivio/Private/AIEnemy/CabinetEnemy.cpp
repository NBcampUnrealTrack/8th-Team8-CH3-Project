#include "AIEnemy/CabinetEnemy.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "OblivioCharacter.h"

DEFINE_LOG_CATEGORY_STATIC(LogCabinetEnemy, Log, All);

namespace
{
	FString GetMontageSlotNames(UAnimMontage* Montage)
	{
		if (!Montage)
		{
			return TEXT("(none)");
		}

		TArray<FString> SlotNames;
		for (const FSlotAnimationTrack& SlotTrack : Montage->SlotAnimTracks)
		{
			SlotNames.Add(SlotTrack.SlotName.ToString());
		}

		return SlotNames.IsEmpty() ? TEXT("(none)") : FString::Join(SlotNames, TEXT(", "));
	}

	void ConfigurePersistedCabinetMeshCollision(UStaticMeshComponent* MeshComp)
	{
		if (!MeshComp)
		{
			return;
		}

		MeshComp->SetMobility(EComponentMobility::Static);
		MeshComp->SetCollisionObjectType(ECC_WorldStatic);
		MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		MeshComp->SetCollisionProfileName(TEXT("BlockAll"));
		MeshComp->SetGenerateOverlapEvents(false);
		MeshComp->SetCanEverAffectNavigation(true);
	}

	bool TransferStaticMeshComponentToProp(
		UStaticMeshComponent* MeshComp,
		AActor* CabinetProp,
		const FTransform& DesiredWorldTransform,
		USceneComponent* AttachParent,
		const FTransform& RelativeTransform,
		bool bAttachToParent)
	{
		if (!IsValid(MeshComp) || !IsValid(CabinetProp))
		{
			return false;
		}

		MeshComp->UnregisterComponent();
		MeshComp->Rename(nullptr, CabinetProp, REN_DontCreateRedirectors | REN_ForceNoResetLoaders);
		CabinetProp->AddInstanceComponent(MeshComp);
		ConfigurePersistedCabinetMeshCollision(MeshComp);

		if (bAttachToParent && AttachParent)
		{
			MeshComp->SetupAttachment(AttachParent);
			MeshComp->SetRelativeTransform(RelativeTransform);
		}
		else
		{
			MeshComp->SetWorldTransform(DesiredWorldTransform);
		}

		MeshComp->RegisterComponent();
		MeshComp->RecreatePhysicsState();
		return true;
	}
}

ACabinetEnemy::ACabinetEnemy()
{
	bAutoStartOpeningCinematic = false;
	PrimaryActorTick.bCanEverTick = true;
	bEnableLightTracking = false;

	CabinetBodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CabinetBodyMesh"));
	CabinetBodyMesh->SetupAttachment(RootComponent);
	CabinetBodyMesh->SetCollisionProfileName(TEXT("BlockAll"));
	CabinetBodyMesh->SetGenerateOverlapEvents(false);

	CabinetDoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CabinetDoorMesh"));
	CabinetDoorMesh->SetupAttachment(CabinetBodyMesh);
	CabinetDoorMesh->SetCollisionProfileName(TEXT("BlockAll"));
	CabinetDoorMesh->SetGenerateOverlapEvents(false);

	EncounterTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("EncounterTrigger"));
	EncounterTrigger->SetupAttachment(CabinetBodyMesh);
	EncounterTrigger->SetBoxExtent(TriggerActivationExtent);
	EncounterTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	EncounterTrigger->SetCollisionObjectType(ECC_WorldDynamic);
	EncounterTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	EncounterTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	EncounterTrigger->SetGenerateOverlapEvents(true);
}

UAnimSequence* ACabinetEnemy::ResolveGrabAnimSequence(UAnimMontage* Montage, UAnimSequence* FallbackSequence)
{
	if (IsValid(FallbackSequence))
	{
		return FallbackSequence;
	}

	if (!IsValid(Montage))
	{
		return nullptr;
	}

	if (UAnimSequenceBase* FirstRef = Montage->GetFirstAnimReference())
	{
		return Cast<UAnimSequence>(FirstRef);
	}

	return nullptr;
}

void ACabinetEnemy::BeginPlay()
{
	Super::BeginPlay();

	SetTargetActor(nullptr);
	StopEnemyMovement();
	SetEnemyState(EEnemyAIState::Idle, true);

	bPlayerArmedEncounterTrigger = true;

	if (EncounterTrigger)
	{
		EncounterTrigger->SetBoxExtent(TriggerActivationExtent);
		EncounterTrigger->OnComponentBeginOverlap.AddDynamic(this, &ACabinetEnemy::OnEncounterTriggerOverlapBegin);
		EncounterTrigger->OnComponentEndOverlap.AddDynamic(this, &ACabinetEnemy::OnEncounterTriggerOverlapEnd);

		if (bRequireExitBeforeEnter)
		{
			TArray<AActor*> OverlappingActors;
			EncounterTrigger->GetOverlappingActors(OverlappingActors, AOblivioCharacter::StaticClass());
			for (AActor* Actor : OverlappingActors)
			{
				if (AOblivioCharacter* Player = Cast<AOblivioCharacter>(Actor))
				{
					if (DoesPlayerQualifyForEncounterTrigger(Player, Player->GetCapsuleComponent()))
					{
						bPlayerArmedEncounterTrigger = false;
						break;
					}
				}
			}
		}

		if (bAllowBeginPlayOverlapStart && bPlayerArmedEncounterTrigger)
		{
			TArray<AActor*> OverlappingActors;
			EncounterTrigger->GetOverlappingActors(OverlappingActors, AOblivioCharacter::StaticClass());
			for (AActor* Actor : OverlappingActors)
			{
				if (AOblivioCharacter* Player = Cast<AOblivioCharacter>(Actor))
				{
					if (DoesPlayerQualifyForEncounterTrigger(Player, Player->GetCapsuleComponent()))
					{
						TryBeginEncounterForPlayer(Player);
						break;
					}
				}
			}
		}
	}

	CacheCabinetDoorClosedRotation();

	if (USkeletalMeshComponent* SkelMesh = GetMesh())
	{
		if (bEnemyHiddenInCabinet)
		{
			SkelMesh->SetHiddenInGame(true);
			SkelMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		}
	}
}

void ACabinetEnemy::Tick(float DeltaSeconds)
{
	if (bCabinetDoorOpenActive)
	{
		UpdateCabinetDoorRotation(DeltaSeconds);
	}

	if (bGrabApproachActive)
	{
		UpdateGrabApproach(DeltaSeconds);
	}
	else if (bHoldGrabApproachPosition)
	{
		EnforceGrabApproachPositionLock();
	}

	EnforceGrabPlayerTransformLock();

	// 연출 전용 — EnemyBase 추격·손전등 추적 AI 는 사용하지 않음.
}

EEnemyAIState ACabinetEnemy::GetEnemyState() const
{
	if (StagingState == EStagingEnemyCinematicState::Dead)
	{
		return EEnemyAIState::Dead;
	}

	if (bCinematicModeActive || bEnemyHiddenInCabinet || !bEncounterTriggered)
	{
		return EEnemyAIState::Idle;
	}

	return AStagingEnemy::GetEnemyState();
}

void ACabinetEnemy::OnEncounterTriggerOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	(void)OverlappedComponent;
	(void)OtherBodyIndex;
	(void)bFromSweep;
	(void)SweepResult;

	AOblivioCharacter* Player = Cast<AOblivioCharacter>(OtherActor);
	if (!DoesPlayerQualifyForEncounterTrigger(Player, OtherComp))
	{
		return;
	}

	if (bRequireExitBeforeEnter && !bPlayerArmedEncounterTrigger)
	{
		UE_LOG(LogCabinetEnemy, Verbose, TEXT("%s: encounter blocked — player must leave trigger first"), *GetName());
		return;
	}

	TryBeginEncounterForPlayer(Player);
}

void ACabinetEnemy::OnEncounterTriggerOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	(void)OverlappedComponent;
	(void)OtherBodyIndex;

	AOblivioCharacter* Player = Cast<AOblivioCharacter>(OtherActor);
	if (!DoesPlayerQualifyForEncounterTrigger(Player, OtherComp))
	{
		return;
	}

	bPlayerArmedEncounterTrigger = true;
}

bool ACabinetEnemy::DoesPlayerQualifyForEncounterTrigger(const AOblivioCharacter* Player,
	const UPrimitiveComponent* OverlapComponent) const
{
	if (!IsValid(Player) || !Player->IsPlayerControlled() || !Player->IsAlive())
	{
		return false;
	}

	if (bRequirePlayerCapsuleOverlap)
	{
		const UCapsuleComponent* PlayerCapsule = Player->GetCapsuleComponent();
		if (!IsValid(PlayerCapsule) || OverlapComponent != PlayerCapsule)
		{
			return false;
		}
	}

	return EncounterTrigger != nullptr;
}

bool ACabinetEnemy::TryBeginEncounterForPlayer(AOblivioCharacter* Player)
{
	if (bOneShotEncounter && bEncounterTriggered)
	{
		return false;
	}

	if (!IsAlive() || bCinematicModeActive)
	{
		return false;
	}

	if (!IsValid(Player) || !Player->IsPlayerControlled() || !Player->IsAlive())
	{
		return false;
	}

	if (!Player->IsLocallyControlled())
	{
		return false;
	}

	if (!DoesPlayerQualifyForEncounterTrigger(Player, Player->GetCapsuleComponent()))
	{
		return false;
	}

	ApplyGrabPairCollisionSuppression(Player);

	bEncounterTriggered = true;

	if (EncounterTrigger)
	{
		EncounterTrigger->SetGenerateOverlapEvents(false);
	}

	UE_LOG(LogCabinetEnemy, Log, TEXT("%s: encounter started for %s"), *GetName(), *Player->GetName());

	DetachCabinetVisualsToWorld();
	OpenCabinetDoor();
	RevealEnemyFromCabinet();
	OnCabinetOpened();
	StartCabinetEncounter(Player);
	PlayCabinetGrabMontage();
	return true;
}

void ACabinetEnemy::RevealEnemyFromCabinet()
{
	bEnemyHiddenInCabinet = false;

	if (USkeletalMeshComponent* SkelMesh = GetMesh())
	{
		SkelMesh->SetHiddenInGame(false);
		SkelMesh->bPauseAnims = false;
		SkelMesh->SetComponentTickEnabled(true);
		SkelMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

		if (SkelMesh->GetAnimationMode() != EAnimationMode::AnimationBlueprint)
		{
			SkelMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		}

		SkelMesh->InitAnim(true);
	}
}

void ACabinetEnemy::CacheCabinetDoorClosedRotation()
{
	if (CabinetDoorMesh)
	{
		CabinetDoorClosedRotation = CabinetDoorMesh->GetRelativeRotation();
		CabinetDoorOpenRotation = CabinetDoorClosedRotation + FRotator(0.f, CabinetDoorOpenYaw, 0.f);
	}
}

bool ACabinetEnemy::IsCabinetVisualMeshComponent(const UStaticMeshComponent* MeshComp) const
{
	return IsValid(MeshComp) && MeshComp != Cast<UStaticMeshComponent>(GetMesh());
}

void ACabinetEnemy::GatherCabinetVisualMeshes(TArray<UStaticMeshComponent*>& OutMeshes) const
{
	OutMeshes.Reset();

	if (!CabinetBodyMesh)
	{
		return;
	}

	OutMeshes.Add(CabinetBodyMesh);

	TArray<USceneComponent*> BodyChildren;
	CabinetBodyMesh->GetChildrenComponents(true, BodyChildren);
	for (USceneComponent* Child : BodyChildren)
	{
		if (UStaticMeshComponent* ChildMesh = Cast<UStaticMeshComponent>(Child))
		{
			OutMeshes.AddUnique(ChildMesh);
		}
	}

	TArray<UStaticMeshComponent*> StaticMeshes;
	GetComponents<UStaticMeshComponent>(StaticMeshes);
	for (UStaticMeshComponent* MeshComp : StaticMeshes)
	{
		if (IsCabinetVisualMeshComponent(MeshComp))
		{
			OutMeshes.AddUnique(MeshComp);
		}
	}
}

void ACabinetEnemy::DetachCabinetVisualsToWorld()
{
	if (bCabinetVisualsDetachedToWorld || !CabinetBodyMesh)
	{
		return;
	}

	TArray<UStaticMeshComponent*> CabinetMeshes;
	GatherCabinetVisualMeshes(CabinetMeshes);

	if (CabinetBodyMesh->GetAttachParent())
	{
		CabinetBodyMesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	}

	CabinetBodyMesh->SetUsingAbsoluteLocation(true);
	CabinetBodyMesh->SetUsingAbsoluteRotation(true);
	CabinetBodyMesh->SetUsingAbsoluteScale(true);

	for (UStaticMeshComponent* MeshComp : CabinetMeshes)
	{
		if (!IsValid(MeshComp) || MeshComp == CabinetBodyMesh)
		{
			continue;
		}

		if (MeshComp->GetAttachParent() == CabinetBodyMesh)
		{
			continue;
		}

		if (MeshComp->GetAttachParent())
		{
			MeshComp->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		}

		MeshComp->SetUsingAbsoluteLocation(true);
		MeshComp->SetUsingAbsoluteRotation(true);
		MeshComp->SetUsingAbsoluteScale(true);
	}

	CachedCabinetBodyWorldTransform = CabinetBodyMesh->GetComponentTransform();
	bCachedCabinetBodyWorldTransform = true;
	bCabinetVisualsDetachedToWorld = true;

	UE_LOG(LogCabinetEnemy, Log, TEXT("%s: cabinet visuals detached to world at %s (meshes=%d)"),
		*GetName(), *CachedCabinetBodyWorldTransform.GetLocation().ToString(), CabinetMeshes.Num());
}

void ACabinetEnemy::PersistCabinetVisualsInWorld()
{
	if (bCabinetVisualsPersistedInWorld || !GetWorld() || !IsValid(CabinetBodyMesh))
	{
		return;
	}

	TArray<UStaticMeshComponent*> MeshesToPersist;
	GatherCabinetVisualMeshes(MeshesToPersist);
	if (MeshesToPersist.Num() == 0)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* const CabinetProp = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), SpawnParams);
	if (!IsValid(CabinetProp))
	{
		return;
	}

#if WITH_EDITOR
	CabinetProp->SetActorLabel(FString::Printf(TEXT("%s_CabinetProp"), *GetName()));
#endif

	const FTransform BodyWorldTransform = bCachedCabinetBodyWorldTransform
		? CachedCabinetBodyWorldTransform
		: CabinetBodyMesh->GetComponentTransform();

	TMap<UStaticMeshComponent*, FTransform> SavedRelativeTransforms;
	TMap<UStaticMeshComponent*, FTransform> SavedWorldTransforms;
	TMap<UStaticMeshComponent*, USceneComponent*> SavedAttachParents;
	for (UStaticMeshComponent* MeshComp : MeshesToPersist)
	{
		if (!IsValid(MeshComp))
		{
			continue;
		}

		SavedRelativeTransforms.Add(MeshComp, MeshComp->GetRelativeTransform());
		SavedWorldTransforms.Add(MeshComp, MeshComp->GetComponentTransform());
		SavedAttachParents.Add(MeshComp, MeshComp->GetAttachParent());
	}

	if (!TransferStaticMeshComponentToProp(
		CabinetBodyMesh,
		CabinetProp,
		BodyWorldTransform,
		nullptr,
		FTransform::Identity,
		false))
	{
		CabinetProp->Destroy();
		return;
	}

	CabinetProp->SetRootComponent(CabinetBodyMesh);

	for (UStaticMeshComponent* MeshComp : MeshesToPersist)
	{
		if (!IsValid(MeshComp) || MeshComp == CabinetBodyMesh)
		{
			continue;
		}

		const bool bAttachToBody = SavedAttachParents.FindRef(MeshComp) == CabinetBodyMesh;
		const FTransform RelativeTransform = SavedRelativeTransforms.FindRef(MeshComp);
		const FTransform WorldTransform = SavedWorldTransforms.FindRef(MeshComp);

		TransferStaticMeshComponentToProp(
			MeshComp,
			CabinetProp,
			WorldTransform,
			CabinetBodyMesh,
			RelativeTransform,
			bAttachToBody);
	}

	if (EncounterTrigger)
	{
		EncounterTrigger->DestroyComponent();
	}

	CabinetBodyMesh = nullptr;
	CabinetDoorMesh = nullptr;
	EncounterTrigger = nullptr;

	bCabinetVisualsPersistedInWorld = true;

	UE_LOG(LogCabinetEnemy, Log, TEXT("%s: cabinet visuals transferred to world prop at %s (meshes=%d)"),
		*GetName(), *BodyWorldTransform.GetLocation().ToString(), MeshesToPersist.Num());
}

void ACabinetEnemy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (EndPlayReason == EEndPlayReason::Destroyed)
	{
		PersistCabinetVisualsInWorld();
	}

	Super::EndPlay(EndPlayReason);
}

void ACabinetEnemy::OpenCabinetDoor()
{
	if (!CabinetDoorMesh || bCabinetDoorOpened || bCabinetDoorOpenActive)
	{
		return;
	}

	CacheCabinetDoorClosedRotation();
	CabinetDoorOpenElapsed = 0.f;
	bCabinetDoorOpenActive = true;

	UE_LOG(LogCabinetEnemy, Log, TEXT("%s: opening cabinet door (yaw %.1f, %.2fs)"),
		*GetName(), CabinetDoorOpenYaw, CabinetDoorOpenDuration);
}

void ACabinetEnemy::UpdateCabinetDoorRotation(float DeltaSeconds)
{
	if (!CabinetDoorMesh || !bCabinetDoorOpenActive)
	{
		return;
	}

	CabinetDoorOpenElapsed += DeltaSeconds;
	const float Alpha = FMath::Clamp(CabinetDoorOpenElapsed / CabinetDoorOpenDuration, 0.f, 1.f);
	const float SmoothAlpha = FMath::InterpEaseInOut(0.f, 1.f, Alpha, 2.f);

	CabinetDoorMesh->SetRelativeRotation(FMath::Lerp(CabinetDoorClosedRotation, CabinetDoorOpenRotation, SmoothAlpha));

	if (Alpha >= 1.f)
	{
		bCabinetDoorOpenActive = false;
		bCabinetDoorOpened = true;
		CabinetDoorMesh->SetRelativeRotation(CabinetDoorOpenRotation);
	}
}

void ACabinetEnemy::StartCabinetEncounter(AOblivioCharacter* Player)
{
	if (!IsValid(Player) || !IsAlive())
	{
		return;
	}

	LinkedPlayer = Player;
	EnterCinematicMode();
	Player->BeginStagingCinematic(this);
	SetStagingState(EStagingEnemyCinematicState::ApproachingGrab);
	StopEnemyMovement();
}

void ACabinetEnemy::PlayCabinetGrabMontage()
{
	USkeletalMeshComponent* SkelMesh = GetMesh();
	if (!SkelMesh)
	{
		UE_LOG(LogCabinetEnemy, Warning, TEXT("%s: no skeletal mesh"), *GetName());
		return;
	}

	UAnimSequence* const GrabSequence = ResolveGrabAnimSequence(CabinetGrabMontage, CabinetGrabAnimSequence);
	if (!IsValid(CabinetGrabMontage) && !IsValid(GrabSequence))
	{
		UE_LOG(LogCabinetEnemy, Warning,
			TEXT("%s: Cabinet Grab Montage / Anim Sequence not assigned — set one on BP_CabinetEnemy"),
			*GetName());
		return;
	}

	SkelMesh->bPauseAnims = false;

	// ABP Slot 미연결 시 몽타주가 재생돼도 T-pose로 보이므로, 연출 구간은 SingleNode로 직접 재생.
	if (IsValid(GrabSequence))
	{
		PlayCinematicAnimSequence(GrabSequence, false);
		UE_LOG(LogCabinetEnemy, Log, TEXT("%s: playing grab sequence %s (SingleNode)"),
			*GetName(), *GetNameSafe(GrabSequence));
		return;
	}

	UAnimInstance* AnimInst = SkelMesh->GetAnimInstance();
	if (!AnimInst)
	{
		UE_LOG(LogCabinetEnemy, Warning, TEXT("%s: no AnimInstance — check Mesh Anim Class (ABP_StagingEnemy)"), *GetName());
		return;
	}

	if (SkelMesh->GetAnimationMode() != EAnimationMode::AnimationBlueprint)
	{
		SkelMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		SkelMesh->InitAnim(true);
		AnimInst = SkelMesh->GetAnimInstance();
	}

	const float Duration = AnimInst->Montage_Play(CabinetGrabMontage);
	if (Duration > 0.f)
	{
		UE_LOG(LogCabinetEnemy, Log, TEXT("%s: playing montage %s (%.2fs, slots: %s)"),
			*GetName(), *GetNameSafe(CabinetGrabMontage), Duration, *GetMontageSlotNames(CabinetGrabMontage));
		return;
	}

	UE_LOG(LogCabinetEnemy, Warning,
		TEXT("%s: Montage_Play failed for %s — montage slots [%s], BP slot '%s'. ABP AnimGraph에 Slot 노드가 있어야 합니다."),
		*GetName(), *GetNameSafe(CabinetGrabMontage), *GetMontageSlotNames(CabinetGrabMontage),
		*GrabMontageSlotName.ToString());
}

void ACabinetEnemy::PlayCinematicAnimSequence(UAnimSequence* Sequence, bool bLoop)
{
	if (!IsValid(Sequence))
	{
		return;
	}

	USkeletalMeshComponent* SkelMesh = GetMesh();
	if (!SkelMesh)
	{
		return;
	}

	SkelMesh->bPauseAnims = false;
	SkelMesh->SetHiddenInGame(false);
	SkelMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	SkelMesh->PlayAnimation(Sequence, bLoop);
	bUsingSingleNodeCinematicAnim = true;

	UE_LOG(LogCabinetEnemy, Log, TEXT("%s: SingleNode %s"), *GetName(), *GetNameSafe(Sequence));
}

void ACabinetEnemy::StopSingleNodeCinematicAnim()
{
	if (!bUsingSingleNodeCinematicAnim)
	{
		return;
	}

	bUsingSingleNodeCinematicAnim = false;

	if (USkeletalMeshComponent* SkelMesh = GetMesh())
	{
		SkelMesh->Stop();
	}
}

void ACabinetEnemy::RestoreCabinetAnimBlueprint()
{
	if (!bUsingSingleNodeCinematicAnim)
	{
		return;
	}

	StopSingleNodeCinematicAnim();

	if (USkeletalMeshComponent* SkelMesh = GetMesh())
	{
		SkelMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		SkelMesh->InitAnim(false);
	}
}

void ACabinetEnemy::BeginMashWindow(int32 InRequiredPressCount)
{
	bMashWindowActive = true;
	CurrentMashPressCount = 0;
	// BP 값과 AnimNotify 값 중 더 큰 쪽 사용 (노티 C++ 기본 5 vs BP 10 불일치 방지)
	RequiredMashPressCount = FMath::Max(1, FMath::Max(InRequiredPressCount, DefaultRequiredMashPressCount));

	SetStagingState(EStagingEnemyCinematicState::MashEscape);

	if (AOblivioCharacter* Player = LinkedPlayer.Get())
	{
		Player->ActivateGrabFirstPersonCamera();
		Player->HandlePlayerCinematicNotify(EPlayerCinematicNotify::EnterStandoff);
		Player->UpdateCabinetMashUI(true, 0, RequiredMashPressCount);
	}

	UE_LOG(LogCabinetEnemy, Log, TEXT("%s: mash window opened — need %d presses (notify=%d, bp=%d)"),
		*GetName(), RequiredMashPressCount, InRequiredPressCount, DefaultRequiredMashPressCount);
}

void ACabinetEnemy::EndMashWindow()
{
	if (!bMashWindowActive)
	{
		return;
	}

	const bool bSuccess = CurrentMashPressCount >= RequiredMashPressCount;
	ResolveMashEscape(bSuccess);
}

void ACabinetEnemy::RegisterMashPress()
{
	if (!bMashWindowActive)
	{
		return;
	}

	++CurrentMashPressCount;
	OnMashProgressChanged.Broadcast(CurrentMashPressCount);

	if (AOblivioCharacter* Player = LinkedPlayer.Get())
	{
		Player->UpdateCabinetMashUI(true, CurrentMashPressCount, RequiredMashPressCount);
	}

	if (CurrentMashPressCount >= RequiredMashPressCount)
	{
		EndMashWindow();
	}
}

void ACabinetEnemy::ResolveMashEscape(bool bSuccess)
{
	bMashWindowActive = false;

	if (AOblivioCharacter* Player = LinkedPlayer.Get())
	{
		Player->UpdateCabinetMashUI(false, 0, 0);
	}

	OnCabinetMashResolved(bSuccess);

	GetWorld()->GetTimerManager().ClearTimer(RestoreAnimBlueprintTimerHandle);

	if (bSuccess)
	{
		ExitCinematicMode();
		StopSingleNodeCinematicAnim();
		ApplyMashEscapeSuccessKnockdown();

		if (AOblivioCharacter* Player = LinkedPlayer.Get())
		{
			Player->EndStagingCinematic();
		}

		UE_LOG(LogCabinetEnemy, Log, TEXT("%s: mash success -> knockdown"), *GetName());
	}
	else
	{
		RestoreCabinetAnimBlueprint();
		ApplyMashFailureToPlayer();
		UE_LOG(LogCabinetEnemy, Log, TEXT("%s: mash failed"), *GetName());
	}

	OnCabinetEncounterConsumed();
	RestoreGrabPairCollisionSuppression();
}

void ACabinetEnemy::ApplyGrabPairCollisionSuppression(AOblivioCharacter* Player)
{
	if (bGrabPairCollisionSuppressed || !IsValid(Player))
	{
		return;
	}

	UCapsuleComponent* EnemyCapsule = GetCapsuleComponent();
	UCapsuleComponent* PlayerCapsule = Player->GetCapsuleComponent();

	if (EnemyCapsule)
	{
		CachedEnemyPawnCollisionResponse = EnemyCapsule->GetCollisionResponseToChannel(ECC_Pawn);
		EnemyCapsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		EnemyCapsule->IgnoreActorWhenMoving(Player, true);
	}

	if (PlayerCapsule)
	{
		CachedPlayerPawnCollisionResponse = PlayerCapsule->GetCollisionResponseToChannel(ECC_Pawn);
		PlayerCapsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		PlayerCapsule->IgnoreActorWhenMoving(this, true);

		if (EnemyCapsule)
		{
			EnemyCapsule->IgnoreComponentWhenMoving(PlayerCapsule, true);
			PlayerCapsule->IgnoreComponentWhenMoving(EnemyCapsule, true);
		}
	}

	if (USkeletalMeshComponent* EnemyMesh = GetMesh())
	{
		if (EnemyMesh->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
		{
			EnemyMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		}
	}

	if (UCharacterMovementComponent* EnemyMove = GetCharacterMovement())
	{
		EnemyMove->StopMovementImmediately();
		EnemyMove->SetMovementMode(MOVE_None);
	}

	if (UCharacterMovementComponent* PlayerMove = Player->GetCharacterMovement())
	{
		bCachedPlayerEnablePhysicsInteraction = PlayerMove->bEnablePhysicsInteraction;
		PlayerMove->bEnablePhysicsInteraction = false;
		PlayerMove->StopMovementImmediately();
		PlayerMove->SetMovementMode(MOVE_None);
	}

	BeginGrabPlayerTransformLock(Player);
	bGrabPairCollisionSuppressed = true;
}

void ACabinetEnemy::BeginGrabPlayerTransformLock(AOblivioCharacter* Player)
{
	if (!IsValid(Player))
	{
		return;
	}

	GrabLockedPlayerLocation = Player->GetActorLocation();
	GrabLockedPlayerRotation = Player->GetActorRotation();
	bLockPlayerTransformDuringGrab = true;
	Player->SetCabinetGrabWorldTransformLock(true, GrabLockedPlayerLocation, GrabLockedPlayerRotation);
}

void ACabinetEnemy::EnforceGrabPlayerTransformLock()
{
	if (!bLockPlayerTransformDuringGrab || bPlayerAttachedForGrab)
	{
		return;
	}

	AOblivioCharacter* Player = LinkedPlayer.Get();
	if (!IsValid(Player))
	{
		return;
	}

	if (!GrabLockedPlayerLocation.Equals(Player->GetActorLocation(), 0.5f))
	{
		Player->SetActorLocation(GrabLockedPlayerLocation, false, nullptr, ETeleportType::TeleportPhysics);
	}

	if (!GrabLockedPlayerRotation.Equals(Player->GetActorRotation(), 0.5f))
	{
		Player->SetActorRotation(GrabLockedPlayerRotation);
	}

	if (UCharacterMovementComponent* PlayerMove = Player->GetCharacterMovement())
	{
		PlayerMove->StopMovementImmediately();
		PlayerMove->Velocity = FVector::ZeroVector;
	}
}

void ACabinetEnemy::ReleaseGrabPlayerTransformLock()
{
	if (AOblivioCharacter* Player = LinkedPlayer.Get())
	{
		Player->SetCabinetGrabWorldTransformLock(false);
	}

	bLockPlayerTransformDuringGrab = false;
}

void ACabinetEnemy::RestoreGrabPairCollisionSuppression()
{
	if (!bGrabPairCollisionSuppressed)
	{
		return;
	}

	ReleaseGrabPlayerTransformLock();

	AOblivioCharacter* Player = LinkedPlayer.Get();

	UCapsuleComponent* EnemyCapsule = GetCapsuleComponent();
	UCapsuleComponent* PlayerCapsule = IsValid(Player) ? Player->GetCapsuleComponent() : nullptr;

	if (EnemyCapsule)
	{
		EnemyCapsule->SetCollisionResponseToChannel(ECC_Pawn, CachedEnemyPawnCollisionResponse);

		if (IsValid(Player))
		{
			EnemyCapsule->IgnoreActorWhenMoving(Player, false);
		}

		if (PlayerCapsule)
		{
			EnemyCapsule->IgnoreComponentWhenMoving(PlayerCapsule, false);
		}
	}

	if (PlayerCapsule)
	{
		PlayerCapsule->SetCollisionResponseToChannel(ECC_Pawn, CachedPlayerPawnCollisionResponse);
		PlayerCapsule->IgnoreActorWhenMoving(this, false);

		if (EnemyCapsule)
		{
			PlayerCapsule->IgnoreComponentWhenMoving(EnemyCapsule, false);
		}
	}

	if (UCharacterMovementComponent* EnemyMove = GetCharacterMovement())
	{
		if (IsAlive() && !bCinematicModeActive)
		{
			EnemyMove->SetMovementMode(MOVE_Walking);
		}
	}

	if (IsValid(Player))
	{
		if (UCharacterMovementComponent* PlayerMove = Player->GetCharacterMovement())
		{
			PlayerMove->bEnablePhysicsInteraction = bCachedPlayerEnablePhysicsInteraction;
			if (Player->IsAlive())
			{
				PlayerMove->SetMovementMode(MOVE_Walking);
			}
		}
	}

	bGrabPairCollisionSuppressed = false;
}

void ACabinetEnemy::HandleGrabComplete()
{
	bHoldGrabApproachPosition = false;
	GrabApproachReferenceForward = FVector::ZeroVector;

	Super::HandleGrabComplete();
	SnapAndAttachLinkedPlayerForGrab();
}

bool ACabinetEnemy::ComputeGrabApproachTarget(FVector& OutEnemyLocation, FRotator& OutEnemyRotation) const
{
	const AOblivioCharacter* Player = LinkedPlayer.Get();
	if (!IsValid(Player))
	{
		return false;
	}

	const FVector PlayerLoc = bLockPlayerTransformDuringGrab
		? GrabLockedPlayerLocation
		: Player->GetActorLocation();
	FVector ReferenceForward = GrabApproachReferenceForward;
	ReferenceForward.Z = 0.f;

	if (ReferenceForward.IsNearlyZero())
	{
		ReferenceForward = Player->GetActorForwardVector();
		ReferenceForward.Z = 0.f;
		if (!ReferenceForward.Normalize())
		{
			ReferenceForward = (GetActorLocation() - PlayerLoc).GetSafeNormal2D();
			if (ReferenceForward.IsNearlyZero())
			{
				return false;
			}
		}
	}

	OutEnemyLocation = PlayerLoc + ReferenceForward * GrabEnemyDistanceInFrontOfPlayer;
	OutEnemyLocation.Z = GetActorLocation().Z;

	const FVector ToPlayer = (PlayerLoc - OutEnemyLocation).GetSafeNormal2D();
	OutEnemyRotation = FRotator(0.f, ToPlayer.Rotation().Yaw, 0.f);
	return true;
}

void ACabinetEnemy::ClampGrabApproachLocation(FVector& InOutLocation) const
{
	const AOblivioCharacter* Player = LinkedPlayer.Get();
	if (!IsValid(Player))
	{
		return;
	}

	const FVector PlayerLoc = bLockPlayerTransformDuringGrab
		? GrabLockedPlayerLocation
		: Player->GetActorLocation();
	FVector ToEnemy = InOutLocation - PlayerLoc;
	ToEnemy.Z = 0.f;

	const float DistSq = ToEnemy.SizeSquared();
	const float MinDistSq = FMath::Square(GrabEnemyDistanceInFrontOfPlayer);
	if (DistSq >= MinDistSq)
	{
		return;
	}

	FVector AwayDir = ToEnemy.GetSafeNormal();
	if (AwayDir.IsNearlyZero())
	{
		AwayDir = GrabApproachReferenceForward;
		AwayDir.Z = 0.f;
		if (AwayDir.IsNearlyZero())
		{
			AwayDir = (GetActorLocation() - PlayerLoc).GetSafeNormal2D();
		}
	}

	InOutLocation = PlayerLoc + AwayDir * GrabEnemyDistanceInFrontOfPlayer;
	InOutLocation.Z = GetActorLocation().Z;
}

void ACabinetEnemy::EnforceGrabApproachPositionLock()
{
	if (!bHoldGrabApproachPosition)
	{
		return;
	}

	SetActorLocation(GrabApproachLockedLocation, false, nullptr, ETeleportType::TeleportPhysics);
	SetActorRotation(GrabApproachLockedRotation);
}

void ACabinetEnemy::BeginGrabApproach(float Duration)
{
	const AOblivioCharacter* Player = LinkedPlayer.Get();
	if (!IsValid(Player))
	{
		return;
	}

	bHoldGrabApproachPosition = false;
	GrabApproachReferenceForward = Player->GetActorForwardVector();
	GrabApproachReferenceForward.Z = 0.f;
	if (!GrabApproachReferenceForward.Normalize())
	{
		GrabApproachReferenceForward = (GetActorLocation() - Player->GetActorLocation()).GetSafeNormal2D();
		if (GrabApproachReferenceForward.IsNearlyZero())
		{
			return;
		}
	}

	if (!ComputeGrabApproachTarget(GrabApproachTargetLocation, GrabApproachTargetRotation))
	{
		return;
	}

	GrabApproachDuration = FMath::Max(Duration, KINDA_SMALL_NUMBER);
	GrabApproachElapsed = 0.f;
	GrabApproachStartLocation = GetActorLocation();
	GrabApproachStartRotation = GetActorRotation();
	bGrabApproachActive = true;

	SetStagingState(EStagingEnemyCinematicState::ApproachingGrab);

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
	}

	UE_LOG(LogCabinetEnemy, Log, TEXT("%s: grab approach started (%.2fs, hold %.0fcm)"),
		*GetName(), GrabApproachDuration, GrabEnemyDistanceInFrontOfPlayer);
}

void ACabinetEnemy::UpdateGrabApproach(float DeltaSeconds)
{
	if (!bGrabApproachActive)
	{
		return;
	}

	GrabApproachElapsed += DeltaSeconds;
	const float Alpha = FMath::Clamp(GrabApproachElapsed / GrabApproachDuration, 0.f, 1.f);
	const float SmoothAlpha = FMath::InterpEaseInOut(0.f, 1.f, Alpha, 2.f);

	const FVector& TargetLocation = GrabApproachTargetLocation;
	const FRotator& TargetRotation = GrabApproachTargetRotation;

	FVector NewLocation = FMath::Lerp(GrabApproachStartLocation, TargetLocation, SmoothAlpha);
	ClampGrabApproachLocation(NewLocation);
	const FRotator NewRotation = FMath::Lerp(GrabApproachStartRotation, TargetRotation, SmoothAlpha);

	SetActorLocation(NewLocation, false, nullptr, ETeleportType::TeleportPhysics);
	SetActorRotation(NewRotation);

	if (Alpha >= 1.f)
	{
		EndGrabApproach();
	}
}

void ACabinetEnemy::EndGrabApproach()
{
	if (!bGrabApproachActive)
	{
		return;
	}

	bGrabApproachActive = false;

	GrabApproachLockedLocation = GetActorLocation();
	GrabApproachLockedRotation = GetActorRotation();
	bHoldGrabApproachPosition = true;

	UE_LOG(LogCabinetEnemy, Log, TEXT("%s: grab approach finished (locked at %.0fcm from player)"),
		*GetName(), GrabEnemyDistanceInFrontOfPlayer);
}

void ACabinetEnemy::SnapAndAttachLinkedPlayerForGrab()
{
	if (!bAttachPlayerDuringGrab)
	{
		return;
	}

	if (bPlayerAttachedForGrab)
	{
		DetachLinkedPlayerFromGrab();
	}

	AOblivioCharacter* Player = LinkedPlayer.Get();
	if (!IsValid(Player))
	{
		return;
	}

	ReleaseGrabPlayerTransformLock();

	if (UCharacterMovementComponent* MoveComp = Player->GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
	}

	const FVector PlayerLoc = Player->GetActorLocation();
	const FVector ToPlayer = (PlayerLoc - GetActorLocation()).GetSafeNormal2D();
	if (ToPlayer.IsNearlyZero())
	{
		return;
	}

	FVector PlayerHoldLoc = GetActorLocation() + ToPlayer * GrabPlayerHoldDistanceFromEnemy;
	PlayerHoldLoc.Z = PlayerLoc.Z;

	if (FVector::DistSquared2D(PlayerLoc, PlayerHoldLoc) > FMath::Square(25.f))
	{
		Player->SetActorLocation(PlayerHoldLoc, false, nullptr, ETeleportType::TeleportPhysics);
	}

	const FVector PlayerToEnemy = (GetActorLocation() - PlayerHoldLoc).GetSafeNormal2D();
	Player->SetActorRotation(FRotator(0.f, PlayerToEnemy.Rotation().Yaw, 0.f));

	USkeletalMeshComponent* EnemyMesh = GetMesh();
	USceneComponent* AttachParent = EnemyMesh ? static_cast<USceneComponent*>(EnemyMesh) : GetRootComponent();
	if (!AttachParent)
	{
		return;
	}

	const FAttachmentTransformRules KeepWorldAttach(
		EAttachmentRule::KeepWorld,
		EAttachmentRule::KeepWorld,
		EAttachmentRule::KeepWorld,
		false);

	if (EnemyMesh && GrabAttachSocketName != NAME_None && EnemyMesh->DoesSocketExist(GrabAttachSocketName))
	{
		Player->AttachToComponent(EnemyMesh, KeepWorldAttach, GrabAttachSocketName);
	}
	else
	{
		Player->AttachToComponent(AttachParent, KeepWorldAttach);
	}

	bPlayerAttachedForGrab = true;

	if (bLockPlayerLookAtEnemyDuringGrab)
	{
		Player->ApplyForcedWorldLookTowards(GetActorLocation(), GrabLookLockDuration);
	}

	UE_LOG(LogCabinetEnemy, Log, TEXT("%s: player attached after grab (hold %.0fcm)"),
		*GetName(), GrabPlayerHoldDistanceFromEnemy);
}

void ACabinetEnemy::DetachLinkedPlayerFromGrab()
{
	if (!bPlayerAttachedForGrab)
	{
		return;
	}

	bPlayerAttachedForGrab = false;

	if (AOblivioCharacter* Player = LinkedPlayer.Get())
	{
		Player->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

		if (UCharacterMovementComponent* MoveComp = Player->GetCharacterMovement())
		{
			MoveComp->SetMovementMode(MOVE_Walking);
		}
	}
}

void ACabinetEnemy::ApplyMashEscapeSuccessKnockdown()
{
	DetachLinkedPlayerFromGrab();
	HandleKnockdownBegin();

	if (AOblivioCharacter* Player = LinkedPlayer.Get())
	{
		Player->HandlePlayerCinematicNotify(EPlayerCinematicNotify::PushSucceeded);
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Green, TEXT("Knocked Down"));
	}

	if (bPlayKnockdownAsSingleNode && IsValid(KnockdownAnimSequence))
	{
		PlayCinematicAnimSequence(KnockdownAnimSequence, false);
	}

	ScheduleKnockdownFinish();
}

void ACabinetEnemy::ScheduleKnockdownFinish()
{
	if (!GetWorld())
	{
		return;
	}

	float FinishDelay = 1.5f;
	if (IsValid(KnockdownAnimSequence))
	{
		FinishDelay = KnockdownAnimSequence->GetPlayLength();
	}

	GetWorld()->GetTimerManager().ClearTimer(RestoreAnimBlueprintTimerHandle);

	TWeakObjectPtr<ACabinetEnemy> WeakThis(this);
	GetWorld()->GetTimerManager().SetTimer(
		RestoreAnimBlueprintTimerHandle,
		[WeakThis]()
		{
			if (!WeakThis.IsValid())
			{
				return;
			}

			WeakThis->RestoreCabinetAnimBlueprint();

			if (WeakThis->bDestroyAfterKnockdown)
			{
				UE_LOG(LogCabinetEnemy, Log, TEXT("%s: destroyed after knockdown"), *WeakThis->GetName());
				WeakThis->PersistCabinetVisualsInWorld();
				WeakThis->Destroy();
			}
		},
		FinishDelay,
		false);
}

void ACabinetEnemy::ApplyMashFailureToPlayer()
{
	if (AOblivioCharacter* Player = LinkedPlayer.Get())
	{
		Player->ApplyHealth(Player->MaxHealth * 10.f);
	}
}

bool ACabinetEnemy::ShouldPlayGrabAnimation() const
{
	return StagingState == EStagingEnemyCinematicState::GrabbedPlayer
		|| StagingState == EStagingEnemyCinematicState::Standoff
		|| StagingState == EStagingEnemyCinematicState::MashEscape;
}

bool ACabinetEnemy::ShouldPlayKnockdownAnimation() const
{
	return StagingState == EStagingEnemyCinematicState::KnockedDown;
}
