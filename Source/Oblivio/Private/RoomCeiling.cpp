#include "RoomCeiling.h"

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"

ARoomCeiling::ARoomCeiling()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.f;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	CeilingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CeilingMesh"));
	CeilingMesh->SetupAttachment(SceneRoot);
	CeilingMesh->SetRelativeLocation(FVector::ZeroVector);
	CeilingMesh->SetRelativeRotation(FRotator::ZeroRotator);
	CeilingMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CeilingMesh->SetCastShadow(true);

	RoomTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("RoomTrigger"));
	RoomTrigger->SetupAttachment(SceneRoot);
	RoomTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	RoomTrigger->SetCollisionObjectType(ECC_WorldDynamic);
	RoomTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	RoomTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	RoomTrigger->SetGenerateOverlapEvents(false);
	RoomTrigger->SetHiddenInGame(true);
	RoomTrigger->ShapeColor = FColor(80, 200, 255);

	ApplyRoomTriggerLayout();
}

static FBox ComputeStaticMeshBoundsInParentSpace(UStaticMeshComponent* MeshComp)
{
	FBox AccumulatedBox(EForceInit::ForceInit);
	if (!IsValid(MeshComp) || !MeshComp->GetStaticMesh())
	{
		return AccumulatedBox;
	}

	const FBoxSphereBounds MeshBounds = MeshComp->GetStaticMesh()->GetBounds();
	const FBox MeshLocalBox(
		MeshBounds.Origin - MeshBounds.BoxExtent,
		MeshBounds.Origin + MeshBounds.BoxExtent);
	const FTransform MeshToParent = MeshComp->GetRelativeTransform();
	const FVector Min = MeshLocalBox.Min;
	const FVector Max = MeshLocalBox.Max;
	for (int32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
	{
		const FVector Corner(
			(CornerIndex & 1) ? Max.X : Min.X,
			(CornerIndex & 2) ? Max.Y : Min.Y,
			(CornerIndex & 4) ? Max.Z : Min.Z);
		AccumulatedBox += MeshToParent.TransformPosition(Corner);
	}

	return AccumulatedBox;
}

void ARoomCeiling::FitRoomTriggerToCeilingMesh()
{
	FitRoomTriggerXYToCeilingMesh();
	ApplyRoomTriggerLayout();
}

void ARoomCeiling::FitRoomTriggerXYToCeilingMesh()
{
	if (!bAutoFitTriggerXYToCeilingMesh || !RoomTrigger)
	{
		return;
	}

	FBox CombinedBounds(EForceInit::ForceInit);

	auto AccumulateMesh = [&CombinedBounds](UStaticMeshComponent* MeshComp)
	{
		const FBox MeshBounds = ComputeStaticMeshBoundsInParentSpace(MeshComp);
		if (MeshBounds.IsValid != 0)
		{
			CombinedBounds += MeshBounds;
		}
	};

	AccumulateMesh(CeilingMesh);
	for (UStaticMeshComponent* ExtraMesh : ExtraCeilingMeshes)
	{
		AccumulateMesh(ExtraMesh);
	}

	if (bAutoCollectAdditionalStaticMeshes)
	{
		TArray<UStaticMeshComponent*> MeshComponents;
		GetComponents<UStaticMeshComponent>(MeshComponents);
		for (UStaticMeshComponent* MeshComp : MeshComponents)
		{
			AccumulateMesh(MeshComp);
		}
	}

	if (CombinedBounds.IsValid == 0)
	{
		return;
	}

	const FVector BoundsCenter = CombinedBounds.GetCenter();
	const FVector BoundsExtent = CombinedBounds.GetExtent();

	RoomTriggerHalfExtent.X = FMath::Max(1.f, BoundsExtent.X - RoomTriggerXYInset);
	RoomTriggerHalfExtent.Y = FMath::Max(1.f, BoundsExtent.Y - RoomTriggerXYInset);

	RoomTriggerRelativeLocation.X = BoundsCenter.X;
	RoomTriggerRelativeLocation.Y = BoundsCenter.Y;
}

void ARoomCeiling::ApplyRoomTriggerLayout()
{
	FitRoomTriggerXYToCeilingMesh();

	if (RoomTrigger)
	{
		RoomTrigger->SetRelativeLocation(RoomTriggerRelativeLocation);
		RoomTrigger->SetBoxExtent(RoomTriggerHalfExtent);
		if (!bDebugDrawRoomTest)
		{
			RoomTrigger->SetHiddenInGame(true);
		}
	}
}

void ARoomCeiling::ApplyDebugVisualization(bool bPlayerInside)
{
	if (!RoomTrigger)
	{
		return;
	}

	if (!bDebugDrawRoomTest)
	{
		RoomTrigger->SetHiddenInGame(true);
		return;
	}

	RoomTrigger->SetHiddenInGame(false);
	RoomTrigger->SetVisibility(true, true);
	RoomTrigger->bDrawOnlyIfSelected = false;
	RoomTrigger->ShapeColor = bPlayerInside ? FColor(255, 40, 40) : FColor(40, 255, 80);
	RoomTrigger->SetLineThickness(12.f);
}

void ARoomCeiling::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyRoomTriggerLayout();
	BuildCeilingPrimitiveList();
	RestoreCeilingForEditor();
}

void ARoomCeiling::BeginPlay()
{
	Super::BeginPlay();

	ApplyRoomTriggerLayout();
	BuildCeilingPrimitiveList();

	if (bUseOpacityFade)
	{
		EnsureFadeMaterialInstances();
		CurrentCeilingOpacity = 1.f;
		TargetCeilingOpacity = 1.f;
	}

	bAnyTrackedPlayerInside = false;
	ApplyInstantCeilingHidden(false);
	UpdatePlayerInsideState();
	ApplyDebugVisualization(bAnyTrackedPlayerInside);
}

void ARoomCeiling::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (RoomTrigger)
	{
		RoomTrigger->SetHiddenInGame(true);
	}

	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(OpacityFadeTimerHandle);
	}

	bAnyTrackedPlayerInside = false;
	ApplyInstantCeilingHidden(false);
	RestoreCeilingForEditor();

	Super::EndPlay(EndPlayReason);
}

void ARoomCeiling::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	PlayerInsidePollAccumSec += DeltaSeconds;
	if (PlayerInsidePollAccumSec >= PlayerInsidePollIntervalSec)
	{
		PlayerInsidePollAccumSec = 0.f;
		UpdatePlayerInsideState();
	}

	if (bDebugDrawRoomTest)
	{
		APawn* const TrackedPawn = FindTrackedPlayerPawn();
		DrawRoomDebug(TrackedPawn, bAnyTrackedPlayerInside);
		ApplyDebugVisualization(bAnyTrackedPlayerInside);
	}
}

void ARoomCeiling::RestoreCeilingForEditor()
{
	for (UPrimitiveComponent* Prim : ResolvedCeilingPrimitives)
	{
		if (!IsValid(Prim))
		{
			continue;
		}
		Prim->SetHiddenInGame(false, true);
		Prim->SetVisibility(true, true);
		Prim->MarkRenderStateDirty();
	}
}

void ARoomCeiling::BuildCeilingPrimitiveList()
{
	ResolvedCeilingPrimitives.Reset();

	auto AddUniquePrimitive = [this](UPrimitiveComponent* Prim)
	{
		if (!IsValid(Prim) || Prim == RoomTrigger)
		{
			return;
		}
		ResolvedCeilingPrimitives.AddUnique(Prim);
	};

	AddUniquePrimitive(CeilingMesh);

	for (UStaticMeshComponent* Extra : ExtraCeilingMeshes)
	{
		AddUniquePrimitive(Extra);
	}

	if (bAutoCollectAdditionalStaticMeshes)
	{
		TArray<UStaticMeshComponent*> MeshComponents;
		GetComponents<UStaticMeshComponent>(MeshComponents);
		for (UStaticMeshComponent* MeshComp : MeshComponents)
		{
			AddUniquePrimitive(MeshComp);
		}
	}

	for (AActor* ExternalActor : ExternalCeilingActors)
	{
		if (!IsValid(ExternalActor))
		{
			continue;
		}

		TArray<UPrimitiveComponent*> PrimitiveComponents;
		ExternalActor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
		for (UPrimitiveComponent* Prim : PrimitiveComponents)
		{
			AddUniquePrimitive(Prim);
		}
	}
}

FVector ARoomCeiling::GetRoomTestLocationForPawn(const APawn* Pawn) const
{
	if (!IsValid(Pawn))
	{
		return FVector::ZeroVector;
	}

	if (bUsePawnFeetForRoomTest)
	{
		if (const UCapsuleComponent* Caps = Pawn->FindComponentByClass<UCapsuleComponent>())
		{
			return Caps->GetComponentLocation() - FVector(0.f, 0.f, Caps->GetScaledCapsuleHalfHeight());
		}
	}

	return Pawn->GetActorLocation();
}

bool ARoomCeiling::IsPawnInsideRoomTrigger(const APawn* Pawn) const
{
	if (!IsValid(Pawn) || !RoomTrigger)
	{
		return false;
	}

	const FVector TestPoint = GetRoomTestLocationForPawn(Pawn);
	const FTransform BoxTM = RoomTrigger->GetComponentTransform();
	const FVector LocalPoint = BoxTM.InverseTransformPosition(TestPoint);

	FVector Extent = RoomTrigger->GetUnscaledBoxExtent();
	Extent.X = FMath::Max(1.f, Extent.X - RoomTriggerXYInset);
	Extent.Y = FMath::Max(1.f, Extent.Y - RoomTriggerXYInset);

	const bool bInsideX = FMath::Abs(LocalPoint.X) <= Extent.X;
	const bool bInsideY = FMath::Abs(LocalPoint.Y) <= Extent.Y;
	if (bIgnoreZForRoomTest)
	{
		return bInsideX && bInsideY;
	}

	const bool bInsideZ = FMath::Abs(LocalPoint.Z) <= Extent.Z;
	return bInsideX && bInsideY && bInsideZ;
}

bool ARoomCeiling::ShouldTrackPawn(const APawn* Pawn) const
{
	if (!IsValid(Pawn))
	{
		return false;
	}

	if (bOnlyAffectLocallyControlledPlayer)
	{
		return Pawn->IsLocallyControlled();
	}

	return Pawn->IsPlayerControlled();
}

APawn* ARoomCeiling::FindTrackedPlayerPawn() const
{
	if (UWorld* const World = GetWorld())
	{
		if (APlayerController* const PC = UGameplayStatics::GetPlayerController(World, 0))
		{
			APawn* const Pawn = PC->GetPawn();
			if (ShouldTrackPawn(Pawn))
			{
				return Pawn;
			}
		}

		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* const PC = It->Get();
			if (!IsValid(PC))
			{
				continue;
			}

			APawn* const Pawn = PC->GetPawn();
			if (ShouldTrackPawn(Pawn))
			{
				return Pawn;
			}
		}
	}

	return nullptr;
}

void ARoomCeiling::UpdatePlayerInsideState()
{
	if (!RoomTrigger)
	{
		return;
	}

	APawn* const TrackedPawn = FindTrackedPlayerPawn();
	const bool bPlayerInside = IsValid(TrackedPawn) && IsPawnInsideRoomTrigger(TrackedPawn);

	bAnyTrackedPlayerInside = bPlayerInside;
	RefreshCeilingVisibility();
}

void ARoomCeiling::DrawRoomDebug(const APawn* TrackedPawn, bool bPlayerInside) const
{
	UWorld* const World = GetWorld();
	if (!RoomTrigger || !World)
	{
		return;
	}

	const FTransform TriggerTransform = RoomTrigger->GetComponentTransform();
	const FVector Extent = RoomTrigger->GetUnscaledBoxExtent();
	const FColor BoxColor = bPlayerInside ? FColor(255, 40, 40) : FColor(40, 255, 80);

	DrawDebugBox(World, TriggerTransform.GetLocation(), Extent, TriggerTransform.GetRotation(),
		BoxColor, false, 0.f, 0, 15.f);

	if (IsValid(TrackedPawn))
	{
		const FVector TestPoint = GetRoomTestLocationForPawn(TrackedPawn);
		const FVector LocalPoint = TriggerTransform.InverseTransformPosition(TestPoint);
		DrawDebugSphere(World, TestPoint, 20.f, 12, FColor::Yellow, false, 0.f, 0, 3.f);

		const FString DebugText = FString::Printf(
			TEXT("%s  Local(%.0f, %.0f, %.0f)"),
			bPlayerInside ? TEXT("INSIDE") : TEXT("OUTSIDE"),
			LocalPoint.X, LocalPoint.Y, LocalPoint.Z);
		DrawDebugString(World, TestPoint + FVector(0.f, 0.f, 40.f),
			DebugText, nullptr, FColor::White, 0.f, true, 1.0f);
	}
}

void ARoomCeiling::RefreshCeilingVisibility()
{
	if (bUseOpacityFade)
	{
		SetOpacityFadeTarget(bAnyTrackedPlayerInside ? 0.f : 1.f);
		return;
	}

	ApplyInstantCeilingHidden(bAnyTrackedPlayerInside);
}

void ARoomCeiling::ApplyInstantCeilingHidden(bool bHideCeiling)
{
	for (UPrimitiveComponent* Prim : ResolvedCeilingPrimitives)
	{
		if (!IsValid(Prim))
		{
			continue;
		}
		Prim->SetHiddenInGame(bHideCeiling, true);
		Prim->SetVisibility(!bHideCeiling, true);
		Prim->MarkRenderStateDirty();
	}
}

void ARoomCeiling::EnsureFadeMaterialInstances()
{
	CeilingFadeMIDs.Reset();

	for (UPrimitiveComponent* Prim : ResolvedCeilingPrimitives)
	{
		if (!IsValid(Prim))
		{
			continue;
		}

		const int32 NumMaterials = Prim->GetNumMaterials();
		for (int32 Slot = 0; Slot < NumMaterials; ++Slot)
		{
			UMaterialInstanceDynamic* MID = Prim->CreateDynamicMaterialInstance(Slot);
			if (IsValid(MID))
			{
				CeilingFadeMIDs.Add(MID);
			}
		}
	}
}

void ARoomCeiling::ApplyOpacityToCeilings(float Opacity)
{
	const float ClampedOpacity = FMath::Clamp(Opacity, 0.f, 1.f);
	CurrentCeilingOpacity = ClampedOpacity;

	for (UMaterialInstanceDynamic* MID : CeilingFadeMIDs)
	{
		if (IsValid(MID))
		{
			MID->SetScalarParameterValue(OpacityMaterialParameterName, ClampedOpacity);
		}
	}

	const bool bFullyHidden = ClampedOpacity <= HiddenOpacityThreshold;
	for (UPrimitiveComponent* Prim : ResolvedCeilingPrimitives)
	{
		if (!IsValid(Prim))
		{
			continue;
		}
		Prim->SetHiddenInGame(bFullyHidden, true);
		Prim->SetVisibility(!bFullyHidden, true);
		Prim->MarkRenderStateDirty();
	}
}

void ARoomCeiling::SetOpacityFadeTarget(float InTargetOpacity)
{
	TargetCeilingOpacity = FMath::Clamp(InTargetOpacity, 0.f, 1.f);

	UWorld* const World = GetWorld();
	if (!World)
	{
		ApplyOpacityToCeilings(TargetCeilingOpacity);
		return;
	}

	if (FMath::IsNearlyEqual(CurrentCeilingOpacity, TargetCeilingOpacity, KINDA_SMALL_NUMBER))
	{
		World->GetTimerManager().ClearTimer(OpacityFadeTimerHandle);
		ApplyOpacityToCeilings(TargetCeilingOpacity);
		return;
	}

	if (!World->GetTimerManager().IsTimerActive(OpacityFadeTimerHandle))
	{
		const float TickInterval = 1.f / 60.f;
		World->GetTimerManager().SetTimer(OpacityFadeTimerHandle, this, &ARoomCeiling::UpdateOpacityFade,
			TickInterval, true, 0.f);
	}
}

void ARoomCeiling::UpdateOpacityFade()
{
	UWorld* const World = GetWorld();
	if (!World)
	{
		return;
	}

	const float InterpSpeed = 1.f / FMath::Max(FadeDuration, KINDA_SMALL_NUMBER);
	const float NewOpacity = FMath::FInterpConstantTo(
		CurrentCeilingOpacity, TargetCeilingOpacity, World->GetDeltaSeconds(), InterpSpeed);
	ApplyOpacityToCeilings(NewOpacity);

	if (FMath::IsNearlyEqual(NewOpacity, TargetCeilingOpacity, KINDA_SMALL_NUMBER))
	{
		World->GetTimerManager().ClearTimer(OpacityFadeTimerHandle);
		ApplyOpacityToCeilings(TargetCeilingOpacity);
	}
}
