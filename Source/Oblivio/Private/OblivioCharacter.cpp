#include "OblivioCharacter.h"
#include "OblivioGameMode.h"
#include "OblivioGameInstance.h"
#include "Notify/PlayerFootstep.h"
#include "Notify/PlayerThrow.h"
#include "OblivioComponents/SoundPropagationComponent.h"
#include "OblivioComponents/PlayerCombatComponent.h"
#include "OblivioComponents/LightAttackComponent.h"
#include "Weapon/Flashlight.h"
#include "Items/OblivioItemBase.h"
#include "Items/OblivioInventoryComponent.h"
#include "Crafting/OblivioCrafting.h"
#include "DoorBase.h"
#include "Memento/FloodLevelActor.h"

#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraComponent.h"
#include "Engine/DamageEvents.h"
#include "EnhancedInputSubsystems.h"
#include "Components/CapsuleComponent.h"
#include "Components/MeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/HitResult.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "DrawDebugHelpers.h"

namespace
{
	static void Obliv_SetMeshOverlayOptional(UPrimitiveComponent* Prim, UMaterialInterface* MatOrNull)
	{
		if (UMeshComponent* MeshComp = Cast<UMeshComponent>(Prim))
		{
			MeshComp->SetOverlayMaterial(MatOrNull);
		}
	}

	static UMaterialInstanceDynamic* Obliv_FindOrAddWallOcclusionFadeMID(
		UPrimitiveComponent* Prim,
		TMap<TWeakObjectPtr<UPrimitiveComponent>, TObjectPtr<UMaterialInstanceDynamic>>& MIDMap)
	{
		if (TObjectPtr<UMaterialInstanceDynamic>* Existing = MIDMap.Find(Prim))
		{
			return Existing->Get();
		}
		return nullptr;
	}
}

AOblivioCharacter::AOblivioCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->SetRelativeRotation(FRotator(-65.f, 0.f, 0.f));
	CameraBoom->TargetArmLength = 900.f;
	CameraBoom->bInheritYaw = false;

	TopDownCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
	TopDownCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);

	FlashlightComponent = CreateDefaultSubobject<USpotLightComponent>(TEXT("Flashlight"));
	FlashlightComponent->SetupAttachment(RootComponent);
	FlashlightComponent->SetRelativeLocation(FVector(40.f, 0.f, 40.f));

	FlashbangLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FlashbangLight"));
	FlashbangLight->SetupAttachment(RootComponent);
	FlashbangLight->SetIntensity(0.0f); // 평소에는 꺼둠
	FlashbangLight->SetCastShadows(false);
	FlashbangLight->SetAttenuationRadius(800.0f);

	CraftingComponent = CreateDefaultSubobject<UOblivioCrafting>(TEXT("CraftingComponent"));

	SoundPropagationComp = CreateDefaultSubobject<USoundPropagationComponent>(TEXT("SoundPropagationComp"));

	CombatComp = CreateDefaultSubobject<UPlayerCombatComponent>(TEXT("CombatComp"));

	GetCharacterMovement()->bOrientRotationToMovement = false;
	bUseControllerRotationYaw = false;

	InventoryComponent = CreateDefaultSubobject<UOblivioInventoryComponent>(TEXT("InventoryComponent"));

	WheelControlMultiplier = 3.f;
}

void AOblivioCharacter::RemoveWallOcclusionVisualFromOccluder(UPrimitiveComponent* Prim)
{
	if (!Prim)
	{
		return;
	}

	WallOcclusionFadeMIDByOccluder.Remove(Prim);
	Obliv_SetMeshOverlayOptional(Prim, nullptr);

	if (UMeshComponent* OccMesh = Cast<UMeshComponent>(Prim))
	{
		if (const TObjectPtr<UMaterialInterface>* SavedEntry = WallOcclusionSavedSwapMaterials.Find(Prim))
		{
			const int32 NumSlots = OccMesh->GetNumMaterials();
			if ((*SavedEntry) != nullptr && NumSlots > 0)
			{
				const int32 SlotIdx = FMath::Clamp(WallOcclusionMaterialSlotToSwap, 0, NumSlots - 1);
				OccMesh->SetMaterial(SlotIdx, SavedEntry->Get());
			}
			WallOcclusionSavedSwapMaterials.Remove(Prim);
		}
	}
}

void AOblivioCharacter::ApplyWallOcclusionVisualToOccluder(UPrimitiveComponent* Prim)
{
	if (!Prim || !IsValid(WallOcclusionOverlayMaterial))
	{
		return;
	}

	if (bWallOcclusionSwapMaterialInsteadOfOverlay)
	{
		if (UMeshComponent* OccMesh = Cast<UMeshComponent>(Prim))
		{
			const int32 NumSlots = OccMesh->GetNumMaterials();
			if (NumSlots > 0)
			{
				const int32 SlotIdx = FMath::Clamp(WallOcclusionMaterialSlotToSwap, 0, NumSlots - 1);
				if (!WallOcclusionSavedSwapMaterials.Contains(Prim))
				{
					WallOcclusionSavedSwapMaterials.Add(Prim, OccMesh->GetMaterial(SlotIdx));
				}

				if (bWallOcclusionDriveLocalizedFadeWithMID)
				{
					UMaterialInstanceDynamic* Mid = Obliv_FindOrAddWallOcclusionFadeMID(Prim, WallOcclusionFadeMIDByOccluder);
					if (!Mid)
					{
						Mid = OccMesh->CreateDynamicMaterialInstance(SlotIdx, WallOcclusionOverlayMaterial);
						if (Mid)
						{
							WallOcclusionFadeMIDByOccluder.Add(Prim, Mid);
						}
					}
					else
					{
						OccMesh->SetMaterial(SlotIdx, Mid);
					}
				}
				else
				{
					WallOcclusionFadeMIDByOccluder.Remove(Prim);
					OccMesh->SetMaterial(SlotIdx, WallOcclusionOverlayMaterial);
				}

				Obliv_SetMeshOverlayOptional(Prim, nullptr);
				return;
			}
		}

		WallOcclusionSavedSwapMaterials.Remove(Prim);
	}
	else
	{
		WallOcclusionSavedSwapMaterials.Remove(Prim);
	}

	if (bWallOcclusionDriveLocalizedFadeWithMID)
	{
		UMaterialInstanceDynamic* Mid = Obliv_FindOrAddWallOcclusionFadeMID(Prim, WallOcclusionFadeMIDByOccluder);
		if (!Mid)
		{
			Mid = UMaterialInstanceDynamic::Create(WallOcclusionOverlayMaterial, this);
			if (Mid)
			{
				WallOcclusionFadeMIDByOccluder.Add(Prim, Mid);
			}
		}

		Obliv_SetMeshOverlayOptional(Prim, Mid ? Mid : WallOcclusionOverlayMaterial);
	}
	else
	{
		WallOcclusionFadeMIDByOccluder.Remove(Prim);
		Obliv_SetMeshOverlayOptional(Prim, WallOcclusionOverlayMaterial);
	}
}

void AOblivioCharacter::ClearWallOcclusionOverlays()
{
	for (const TWeakObjectPtr<UPrimitiveComponent>& Ptr : WallOcclusionAppliedPrimitives)
	{
		if (UPrimitiveComponent* Prev = Ptr.Get())
		{
			RemoveWallOcclusionVisualFromOccluder(Prev);
		}
	}
	WallOcclusionAppliedPrimitives.Empty();
	WallOcclusionSavedSwapMaterials.Reset();
	WallOcclusionFadeMIDByOccluder.Reset();
}

bool AOblivioCharacter::ShouldTreatHitAsOccluderWall(UPrimitiveComponent const* Primitive,
													 FVector const& ImpactNormalWorld) const
{
	if (!Primitive)
	{
		return false;
	}

	const FVector N =
		ImpactNormalWorld.IsNearlyZero() ? FVector::UpVector : ImpactNormalWorld.GetSafeNormal();
	if (!bWallOcclusionIgnoreFloorCeilingNormalFilter
		&& FMath::Abs(N.Z) >= WallOcclusionFloorCeilingCosThreshold)
	{
		return false;
	}

	if (Primitive->GetClass()->GetName().Contains(TEXT("Landscape")))
	{
		return false;
	}

	return Primitive->GetCollisionEnabled() != ECollisionEnabled::NoCollision;
}

bool AOblivioCharacter::ShouldApplyWallOcclusionToPrimitive(UPrimitiveComponent const* Prim) const
{
	if (!bWallOcclusionRestrictToBaseMaterial)
	{
		return true;
	}
	if (!IsValid(WallOcclusionAllowedBaseMaterial))
	{
		return false;
	}

	const UMeshComponent* const OcclMesh = Cast<const UMeshComponent>(Prim);
	if (!OcclMesh)
	{
		return false;
	}

	const int32 NumSlots = OcclMesh->GetNumMaterials();
	if (NumSlots <= 0)
	{
		return false;
	}

	const int32 Slot = FMath::Clamp(WallOcclusionMaterialMatchSlotIndex, 0, NumSlots - 1);
	UMaterialInterface* const SlotMat = OcclMesh->GetMaterial(Slot);
	if (!IsValid(SlotMat))
	{
		return false;
	}

	UMaterial* const SlotBase = SlotMat->GetBaseMaterial();
	UMaterial* const AllowedBase = WallOcclusionAllowedBaseMaterial->GetBaseMaterial();
	return SlotBase != nullptr && SlotBase == AllowedBase;
}

FVector AOblivioCharacter::GetWallOcclusionTraceStartWorld() const
{
	auto WithExtra = [this](const FVector Loc) -> FVector { return Loc + WallOcclusionTraceStartWorldExtraOffset; };

	if (bWallOcclusionTraceStartUsesTopDownCameraWorldLocation)
	{
		if (TopDownCamera)
		{
			return WithExtra(TopDownCamera->GetComponentLocation());
		}
		return WithExtra(GetActorLocation());
	}

	if (!CameraBoom)
	{
		return TopDownCamera ? WithExtra(TopDownCamera->GetComponentLocation()) : WithExtra(GetActorLocation());
	}

	const FTransform BoomWorldTM = CameraBoom->GetComponentTransform();
	const float ArmLen = FMath::Max(1.f, CameraBoom->TargetArmLength);
	const FVector Sock = CameraBoom->SocketOffset;
	// +X / −X 양쪽 후보 중 실제 TopDown 카메라에 더 가까운 논리 위치 선택(블루프린트 트랜스폼 차이 등으로 카메라가 로컬 −X쪽에 매달린 경우 트레이스가 플레이어 반대쪽으로 새서 벽 미적중).
	const FVector CandPosLocal = FVector(ArmLen, 0.f, 0.f) + Sock;
	const FVector CandNegLocal = FVector(-ArmLen, 0.f, 0.f) + Sock;
	const FVector WorldPos = BoomWorldTM.TransformPosition(CandPosLocal);
	const FVector WorldNeg = BoomWorldTM.TransformPosition(CandNegLocal);

	FVector Candidate;
	if (TopDownCamera)
	{
		const FVector Actual = TopDownCamera->GetComponentLocation();
		const float DPos = FVector::DistSquared(WorldPos, Actual);
		const float DNeg = FVector::DistSquared(WorldNeg, Actual);
		Candidate = DNeg < DPos ? WorldNeg : WorldPos;
	}
	else
	{
		Candidate = WorldPos;
	}

	return WithExtra(Candidate);
}

FVector AOblivioCharacter::BiasWallOcclusionTraceEndTowardsTraceStartWorld(FVector SampleWorldHint) const
{
	if (WallOcclusionTracePullTowardsTraceStartFracOfRadius <= 0.f)
	{
		return SampleWorldHint;
	}

	const FVector Start = GetWallOcclusionTraceStartWorld();
	FVector Dir = Start - SampleWorldHint;
	const float Dist = Dir.Size();
	if (Dist <= KINDA_SMALL_NUMBER)
	{
		return SampleWorldHint;
	}
	Dir /= Dist;

	float CapsRad = 45.f;
	if (UCapsuleComponent const* Capsule = GetCapsuleComponent())
	{
		CapsRad = Capsule->GetScaledCapsuleRadius();
	}

	return SampleWorldHint + Dir * CapsRad * WallOcclusionTracePullTowardsTraceStartFracOfRadius;
}

FVector AOblivioCharacter::GetWallOcclusionFocusWorldLocation() const
{
	FVector Raw;
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		Raw = MeshComp->GetComponentTransform().TransformPosition(WallOcclusionSpineLocalOffsetFromMesh);
	}
	else
	{
		Raw = GetCapsuleComponent() ? GetCapsuleComponent()->GetComponentLocation() : GetActorLocation();
	}

	return BiasWallOcclusionTraceEndTowardsTraceStartWorld(Raw);
}

void AOblivioCharacter::RefreshWallOcclusionFadeMaterialInstances()
{
	if (!bWallOcclusionDriveLocalizedFadeWithMID || !IsLocallyControlled())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World || World->bIsTearingDown)
	{
		return;
	}

	const FVector FocusWorld = GetWallOcclusionFocusWorldLocation();
	const FVector LineStartWorld = GetWallOcclusionTraceStartWorld();
	const float RadiusUU = WallOcclusionFocusRadiusUU;
	const float HalfThickUU = WallOcclusionFocusLineHalfThicknessUU;

	const FLinearColor FocusLinear(FocusWorld.X, FocusWorld.Y, FocusWorld.Z, 1.f);

	for (auto It = WallOcclusionFadeMIDByOccluder.CreateIterator(); It; ++It)
	{
		UPrimitiveComponent* KeyPrim = It.Key().Get();
		UMaterialInstanceDynamic* MID = It.Value().Get();

		const bool bKeyOk = KeyPrim && IsValid(KeyPrim) && KeyPrim->IsRegistered();
		const bool bMidOk = MID && IsValid(MID) && MID->IsValidLowLevelFast()
			&& !MID->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed);
		if (!bKeyOk || !bMidOk)
		{
			It.RemoveCurrent();
			continue;
		}
	}

	const TMap<TWeakObjectPtr<UPrimitiveComponent>, TObjectPtr<UMaterialInstanceDynamic>> CopyMIDMap =
		WallOcclusionFadeMIDByOccluder;
	for (const TPair<TWeakObjectPtr<UPrimitiveComponent>, TObjectPtr<UMaterialInstanceDynamic>>& Elem : CopyMIDMap)
	{
		UPrimitiveComponent* KeyPrim = Elem.Key.Get();
		UMaterialInstanceDynamic* MID = Elem.Value.Get();
		const bool bKeyOk = KeyPrim && IsValid(KeyPrim) && KeyPrim->IsRegistered();
		const bool bMidOk = MID && IsValid(MID) && MID->IsValidLowLevelFast()
			&& !MID->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed);
		if (!bKeyOk || !bMidOk)
		{
			WallOcclusionFadeMIDByOccluder.Remove(Elem.Key);
			continue;
		}

		// FVector 오버로드 대신 LinearColor 경로 사용(파라미터 바인딩 시 내부 null 피하기 위해).
		if (!WallOcclusionMIDParam_FocusWorld.IsNone())
		{
			MID->SetVectorParameterValue(WallOcclusionMIDParam_FocusWorld, FocusLinear);
		}
		if (!WallOcclusionMIDParam_FocusRadiusUU.IsNone())
		{
			MID->SetScalarParameterValue(WallOcclusionMIDParam_FocusRadiusUU, RadiusUU);
		}

		if (bWallOcclusionMIDPassCameraToFocusLineParams)
		{
			const FLinearColor LineStartLinear(LineStartWorld.X, LineStartWorld.Y, LineStartWorld.Z, 1.f);

			if (!WallOcclusionMIDParam_LineStartWorld.IsNone())
			{
				MID->SetVectorParameterValue(WallOcclusionMIDParam_LineStartWorld, LineStartLinear);
			}
			if (!WallOcclusionMIDParam_LineEndWorld.IsNone())
			{
				MID->SetVectorParameterValue(WallOcclusionMIDParam_LineEndWorld, FocusLinear);
			}
			if (!WallOcclusionMIDParam_LineHalfThicknessUU.IsNone())
			{
				MID->SetScalarParameterValue(WallOcclusionMIDParam_LineHalfThicknessUU, HalfThickUU);
			}
		}
	}
}

void AOblivioCharacter::UpdateWallOcclusionDither()
{
	UWorld* World = GetWorld();
	if (!World || !TopDownCamera)
	{
		return;
	}

	if (!IsLocallyControlled())
	{
		if (WallOcclusionAppliedPrimitives.Num() > 0)
		{
			ClearWallOcclusionOverlays();
		}
		return;
	}

#if UE_BUILD_SHIPPING
	static constexpr bool bDbgTraces = false;
	static constexpr bool bDbgGap = false;
#else
	const bool bDbgTraces = bWallOcclusionDebugDrawTraces;
	const bool bDbgGap = bWallOcclusionDebugDrawActualCameraGap;
#endif

	const bool bWantsOcclusionOverlay = bWallOcclusionDitherEnabled && IsValid(WallOcclusionOverlayMaterial);

	if (!bWantsOcclusionOverlay && WallOcclusionAppliedPrimitives.Num() > 0)
	{
		ClearWallOcclusionOverlays();
	}

	if (!bWantsOcclusionOverlay && !bDbgTraces && !bDbgGap)
	{
		return;
	}

	const bool bBypassIntervalForDebug = bDbgTraces || bDbgGap;
	const double Now = World->GetTimeSeconds();
	if (!bBypassIntervalForDebug && WallOcclusionUpdateIntervalSec > 0.f && Now < WallOcclusionNextUpdateWorldTimeSeconds)
	{
		return;
	}
	if (!bBypassIntervalForDebug && WallOcclusionUpdateIntervalSec > 0.f)
	{
		WallOcclusionNextUpdateWorldTimeSeconds = Now + static_cast<double>(WallOcclusionUpdateIntervalSec);
	}

	const FVector TraceStartWorld = GetWallOcclusionTraceStartWorld();

	TArray<FVector, TInlineAllocator<8>> TraceEndWorldSamples;
	if (UCapsuleComponent const* Caps = GetCapsuleComponent())
	{
		const FVector Feet = GetActorLocation();
		const float HH = Caps->GetUnscaledCapsuleHalfHeight();
		const float Radius = Caps->GetScaledCapsuleRadius();

		auto XYBiasTowardCamera = [&](const FVector WorldPointSameZFoot)
		{
			FVector ToCam(TraceStartWorld.X - WorldPointSameZFoot.X, TraceStartWorld.Y - WorldPointSameZFoot.Y, 0.f);
			float LenXY = ToCam.Size();
			if (LenXY <= KINDA_SMALL_NUMBER)
			{
				return FVector(WorldPointSameZFoot.X, WorldPointSameZFoot.Y, WorldPointSameZFoot.Z);
			}
			ToCam /= LenXY;
			return FVector(
				WorldPointSameZFoot.X + ToCam.X * Radius * 0.4f,
				WorldPointSameZFoot.Y + ToCam.Y * Radius * 0.4f,
				WorldPointSameZFoot.Z);
		};

		const FVector Low = Feet + FVector(0.f, 0.f, HH * 0.25f);
		const FVector HighAnch = Feet + FVector(0.f, 0.f, HH * 0.85f);
		TraceEndWorldSamples.Add(BiasWallOcclusionTraceEndTowardsTraceStartWorld(Low));
		TraceEndWorldSamples.Add(BiasWallOcclusionTraceEndTowardsTraceStartWorld(XYBiasTowardCamera(HighAnch)));
	}
	else
	{
		TraceEndWorldSamples.Add(GetActorLocation());
	}

	if (USkeletalMeshComponent* SkMesh = GetMesh())
	{
		TraceEndWorldSamples.Add(BiasWallOcclusionTraceEndTowardsTraceStartWorld(
			SkMesh->GetComponentTransform().TransformPosition(WallOcclusionSpineLocalOffsetFromMesh)));
	}
	for (const FVector& RelLocal : WallOcclusionExtraTraceLocals)
	{
		TraceEndWorldSamples.Add(BiasWallOcclusionTraceEndTowardsTraceStartWorld(GetActorTransform().TransformPosition(RelLocal)));
	}

#if !UE_BUILD_SHIPPING
	static constexpr float WODbgLineThick = 2.f;
	const float DbgLifeDraw = WallOcclusionDebugTraceLifeSec > KINDA_SMALL_NUMBER ? WallOcclusionDebugTraceLifeSec : 0.f;

	if (bDbgGap)
	{
		const FVector ActualCamLoc = TopDownCamera->GetComponentLocation();
		DrawDebugLine(World, ActualCamLoc, TraceStartWorld, FColor::Silver, false, DbgLifeDraw, 0, WODbgLineThick);
		DrawDebugSphere(World, ActualCamLoc, 12.f, 8, FColor::Orange, false, DbgLifeDraw);
		DrawDebugSphere(World, TraceStartWorld, 14.f, 8, FColor::Yellow, false, DbgLifeDraw);
	}

	if (bDbgTraces)
	{
		if (!bDbgGap)
		{
			DrawDebugSphere(World, TraceStartWorld, 18.f, 10, FColor::Yellow, false, DbgLifeDraw);
		}
		static const FColor LineColors[] = {
			FColor::Cyan,
			FColor::Magenta,
			FColor(46, 204, 113), // emerald-ish
			FColor::Orange,
			FColor(180, 80, 255),
			FColor(80, 200, 255),
			FColor(255, 200, 50),
			FColor(100, 255, 180),
		};
		const int NumColors = UE_ARRAY_COUNT(LineColors);
		int SampleIdx = 0;
		for (const FVector& TraceEndWorld : TraceEndWorldSamples)
		{
			const FColor HC = LineColors[SampleIdx % NumColors];
			DrawDebugLine(World, TraceStartWorld, TraceEndWorld, HC, false, DbgLifeDraw, 0, WODbgLineThick);
			DrawDebugSphere(World, TraceEndWorld, 10.f, 8, HC, false, DbgLifeDraw);
			++SampleIdx;
		}
	}
#endif

	if (!bWantsOcclusionOverlay)
	{
		return;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(WallOcclusionDitherTrace), false, this);
	Params.AddIgnoredActor(this);
	if (IsValid(CurrentWeapon))
	{
		Params.AddIgnoredActor(CurrentWeapon);
	}

	TSet<UPrimitiveComponent*> HitOccludersPrimitives;
	for (const FVector& TraceEndWorld : TraceEndWorldSamples)
	{
		TArray<FHitResult> Hits;
		World->LineTraceMultiByChannel(
			Hits, TraceStartWorld, TraceEndWorld, WallOcclusionTraceChannel.GetValue(), Params);

		const float SegmentLen = FVector::Dist(TraceStartWorld, TraceEndWorld);
		const float MaxHitDistance = SegmentLen + WallOcclusionIgnoreBeyondBodyMarginUU;
#if !UE_BUILD_SHIPPING
		const float DbgHitLife = WallOcclusionDebugTraceLifeSec > KINDA_SMALL_NUMBER ? WallOcclusionDebugTraceLifeSec : 0.f;
#endif
		for (const FHitResult& Hit : Hits)
		{
			if (!Hit.HasValidHitObjectHandle())
			{
				continue;
			}
			if (Hit.Distance > MaxHitDistance)
			{
				break;
			}
			UPrimitiveComponent* Comp = Hit.GetComponent();
			const bool bAcceptOcc = ShouldTreatHitAsOccluderWall(Comp, Hit.ImpactNormal);
#if !UE_BUILD_SHIPPING
			if (bDbgTraces && bAcceptOcc)
			{
				DrawDebugPoint(World, Hit.ImpactPoint, 14.f, FColor::Purple, false, DbgHitLife);
			}
#endif
			if (!bAcceptOcc)
			{
				continue;
			}
			if (!ShouldApplyWallOcclusionToPrimitive(Comp))
			{
				continue;
			}
			HitOccludersPrimitives.Add(Comp);
		}
	}

	for (auto It = WallOcclusionAppliedPrimitives.CreateIterator(); It; ++It)
	{
		TWeakObjectPtr<UPrimitiveComponent> PrevWeak = *It;
		UPrimitiveComponent* Prev = PrevWeak.Get();
		if (!Prev || !HitOccludersPrimitives.Contains(Prev))
		{
			if (Prev)
			{
				RemoveWallOcclusionVisualFromOccluder(Prev);
			}
			It.RemoveCurrent();
		}
	}

	for (UPrimitiveComponent* HitPrim : HitOccludersPrimitives)
	{
		if (!HitPrim)
		{
			continue;
		}
		const TWeakObjectPtr<UPrimitiveComponent> HitWeak(HitPrim);
		if (!WallOcclusionAppliedPrimitives.Contains(HitWeak))
		{
			ApplyWallOcclusionVisualToOccluder(HitPrim);
			WallOcclusionAppliedPrimitives.Add(HitWeak);
		}
	}
}

void AOblivioCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (CameraBoom)
	{
		CameraBoom->bDoCollisionTest = !bWallOcclusionDisableSpringArmProbe;
	}

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StartCameraFade(1.0f, 0.0f, 1.5f, FLinearColor::Black, false, true);
		}
	}

	UpdateFlashlightVisuals();

	//8층부터는 현재 스탯을 인스턴스에 저장된 것으로 대체
	UOblivioGameInstance* GI = Cast<UOblivioGameInstance>(GetGameInstance());
	if (!GI) return;

	if (GI->CurrentFloor < 9)
	{
		CurrentHealth = GI->CurrentHealth;
		Battery = GI->CurrentBattery;
		Hunger = GI->CurrentHunger;
		Thirst = GI->CurrentThirst;
	}

	//시작시 손전등 장착
	if (IsValid(FlashlightWeapon)) {
		UE_LOG(LogTemp, Warning, TEXT("Spawning Weapon"));
		FActorSpawnParameters Params;
		Params.Owner = this;
		CurrentWeapon = GetWorld()->SpawnActor<AWeaponBase>(FlashlightWeapon, GetActorTransform(), Params);
		if (IsValid(CurrentWeapon)) {
			UE_LOG(LogTemp, Warning, TEXT("Attaching Weapon"));
			CurrentWeapon->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, FName("WeaponSocket"));
		}

	}
	//기존 기본부착 손전등 off
	FlashlightComponent->SetVisibility(false);

	//AnimNotify 델리게이트 장착
	OnPlayerFootstep.AddDynamic(this, &AOblivioCharacter::GenerateFootstep);
	OnPlayerThrow.AddDynamic(this, &AOblivioCharacter::ThrowWeapon);
	bIsThrowing = false;
}

void AOblivioCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearWallOcclusionOverlays();
	Super::EndPlay(EndPlayReason);
}

//==========================
// Tick and Status
//==========================

void AOblivioCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateStatus(DeltaTime);
	UpdateWallOcclusionDither();
	RefreshWallOcclusionFadeMaterialInstances();
	//Debug 확인용
	if (GEngine)
	{
		//생존 스탯 (Health, Hunger, Thirst)
		FString StatusMsg = FString::Printf(TEXT("HP: %.1f | Hunger: %.1f | Thirst: %.1f"), CurrentHealth, Hunger, Thirst);
		GEngine->AddOnScreenDebugMessage(1, DeltaTime, FColor::Cyan, StatusMsg);

		//배터리 상태 및 손전등 ON/OFF
		FString BatteryMsg = FString::Printf(TEXT("Battery: %.1f%% (%s) | Focus: %.2f"),
			Battery, bIsFlashlightOn ? TEXT("ON") : TEXT("OFF"), CurrentFocusAlpha);

		// 배터리가 적으면 빨간색, 충분하면 초록색으로 표시
		FColor BatteryColor = (Battery < 20.f) ? FColor::Red : FColor::Green;
		GEngine->AddOnScreenDebugMessage(2, DeltaTime, BatteryColor, BatteryMsg);

		//이동 상태
		FString MoveMsg = FString::Printf(TEXT("Movement: %s | Speed: %.1f"),
			bIsRunning ? TEXT("RUNNING") : TEXT("WALKING"), GetVelocity().Size());
		GEngine->AddOnScreenDebugMessage(3, DeltaTime, FColor::Yellow, MoveMsg);
	}
	if (IsValid(CurrentWeapon)) {
		CurrentWeapon->SetActorRotation(GetActorRotation());
	}
	UpdateFlashlightEmbedPullback(DeltaTime);
}

void AOblivioCharacter::UpdateStatus(float DeltaTime)
{
	if (bIsDead) return;

	float DepleteRate = bIsRunning ? 2.0f : 1.0f;
	Hunger = FMath::Max(0.0f, Hunger - (DeltaTime * 0.3f * DepleteRate));
	Thirst = FMath::Max(0.0f, Thirst - (DeltaTime * 0.4f * DepleteRate));

	// 배터리 처리
	if (bIsFlashlightOn && Battery > 0.0f)
	{
		float FocusPenalty = FMath::Lerp(1.0f, 1.5f, CurrentFocusAlpha);
		if (!bCheatInfiniteBattery)
		{
			Battery = FMath::Max(0.0f, Battery - (DeltaTime * BatteryDepletionRate * FocusPenalty));
		}

		if (Battery <= 0.0f)
		{
			bIsFlashlightOn = false;
			UpdateFlashlightVisuals();
		}
	}

	// 굶주림/갈증으로 인한 체력 감소
	if (Hunger <= 0.0f || Thirst <= 0.0f)
	{
		ApplyHealth(DeltaTime * 1.0f);
	}

	GetCharacterMovement()->MaxWalkSpeed = bIsRunning ? RunSpeed : WalkSpeed;

	if (bIsStunned)
	{
		// 스턴 상태면 아예 움직이지 못함
		GetCharacterMovement()->MaxWalkSpeed = 0.0f;
	}
	else
	{
		float BaseSpeed = bIsRunning ? RunSpeed : WalkSpeed;
		GetCharacterMovement()->MaxWalkSpeed = bIsSlowed ? (BaseSpeed * CurrentSlowMultiplier) : BaseSpeed;
	}

	// [2층 기믹 추가] 수중 상태 확인
	bool bIsInWater = IsInWater();
	float WaterSpeedMultiplier = bIsInWater ? 0.5f : 1.0f; // 수중에서는 50% 감속

	if (bIsStunned)
	{
		GetCharacterMovement()->MaxWalkSpeed = 0.0f;
	}
	else
	{
		float BaseSpeed = bIsRunning ? RunSpeed : WalkSpeed;

		//수중Multiplier와 기존 슬로우Multiplier를 모두 적용
		float FinalSpeed = BaseSpeed * WaterSpeedMultiplier;
		if (bIsSlowed) FinalSpeed *= CurrentSlowMultiplier;

		GetCharacterMovement()->MaxWalkSpeed = FinalSpeed;
	}

	// 수중 이동 시 첨벙거리는 소리로 적에게 위치 노출
	if (bIsInWater && GetVelocity().Size() > 10.f)
	{
		if (IsValid(SoundPropagationComp))
		{
			// 물결 소리 전파
			SoundPropagationComp->PropagateSound();
		}
	}
}

//==========================
// 입력 and 상호작용
//==========================

void AOblivioCharacter::Move(const FVector2D& Value)
{
	if (Controller != nullptr)
	{
		const float Dir = bMovementInverted ? -1.0f : 1.0f;
		AddMovementInput(FVector::ForwardVector, Value.Y * Dir);
		AddMovementInput(FVector::RightVector,   Value.X * Dir);
	}
}

void AOblivioCharacter::StartRunning() { bIsRunning = true; }
void AOblivioCharacter::StopRunning() { bIsRunning = false; }


void AOblivioCharacter::ToggleInventory()
{
	bIsInventoryOpen = !bIsInventoryOpen;
	
	OnInventoryToggle(bIsInventoryOpen);
}

void AOblivioCharacter::ToggleCrafting()
{
	if (CraftingComponent)
	{
		CraftingComponent->ToggleCraftingMode();
		bIsCraftingOpen = CraftingComponent->bIsCraftingModeActive;
		
		OnCraftingUIToggle(bIsCraftingOpen);
	}
}

void AOblivioCharacter::Interact()
{
	// 상호작용할 대상(TargetActor) 하나만 확실하게 정하기
	AActor* TargetActor = nullptr;

	// 우선순위 1: 발밑에 가까이 있는 아이템 (Overlap)
	if (CurrentNearbyItem)
	{
		TargetActor = CurrentNearbyItem;
	}
	else
	{
		// 우선순위 2: 크로스헤어로 쳐다보는 대상 (LineTrace)
		FHitResult HitResult;
		FVector Start = GetActorLocation();
		FVector End = Start + (GetActorForwardVector() * InteractionDistance);
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(this);

		DrawDebugLine(GetWorld(), Start, End, FColor::Red, false, 1.0f, 0, 2.0f);

		if (GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, Params))
		{
			TargetActor = HitResult.GetActor();
			UE_LOG(LogTemp, Warning, TEXT("1. Hit Something: %s"), *TargetActor->GetName());
		}
	}

	// 상호작용할 대상이 아무것도 없으면 그대로 종료
	if (!TargetActor) return;


	// 문(Door) 상호작용
	if (ADoorBase* HitDoor = Cast<ADoorBase>(TargetActor))
	{
		HitDoor->InteractDoor();
		return;
	}


	// 태그 기반 상호작용 (유품, 열쇠, 체크포인트)
	AOblivioGameMode* GM = Cast<AOblivioGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
	if (GM)
	{
		if (TargetActor->ActorHasTag("Memento"))
		{
			GM->AddMemento();
			UE_LOG(LogTemp, Warning, TEXT("Get Memento!"));

			// 홍수 트리거 확인
			if (TargetActor->ActorHasTag("FloodTrigger"))
			{
				GM->TriggerFloodEvent();
			}

			if (TargetActor == CurrentNearbyItem) SetNearbyItem(nullptr);
			TargetActor->Destroy();
			return;
		}
		else if (TargetActor->ActorHasTag("RestArea"))
		{
			GM->RestInteraction();
			return;
		}
	}


	// 4. 일반 아이템 (인벤토리 추가)
	if (AOblivioItemBase* PickedItem = Cast<AOblivioItemBase>(TargetActor))
	{
		PickedItem->OnInteract(this);

		if (InventoryComponent && InventoryComponent->AddItem(PickedItem))
		{
			if (PickedItem->ActorHasTag("Key") && GM)
			{
				GM->CollectedKeys++;
				UE_LOG(LogTemp, Warning, TEXT("Key Added to Inventory! Current: %d / %d"), GM->CollectedKeys, GM->RequiredKeys);
			}
			if (TargetActor == CurrentNearbyItem) SetNearbyItem(nullptr);
			PickedItem->Destroy();
			UE_LOG(LogTemp, Warning, TEXT("3. Item Added to Inventory and Destroyed!"));
			return;
		}
	}
}

void AOblivioCharacter::SetNearbyItem(AOblivioItemBase* Item)
{
	CurrentNearbyItem = Item;
	// UI 바인딩 필요 press e
	OnNearbyItemChanged.Broadcast(CurrentNearbyItem);
}

void AOblivioCharacter::TogglePause()
{
	//UI 띄우는 로직 연동
	UE_LOG(LogTemp, Warning, TEXT("Pause Menu Toggled!"));
	
	bIsPauseOpen = !bIsPauseOpen;
	
	OnPauseToggle(bIsPauseOpen);

	// 만약 직접 엔진 일시정지 제어 시
	/*
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC)
	{
		bool bIsPaused = UGameplayStatics::IsGamePaused(GetWorld());
		UGameplayStatics::SetGamePaused(GetWorld(), !bIsPaused);
		PC->SetShowMouseCursor(!bIsPaused);
	}
	*/
}

//=====================
//무기 장비
//======================

void AOblivioCharacter::AdjustFocus(float Value)
{
	if (bCanAdjustFocus && CurrentWeapon)
	{
		CurrentWeapon->ChangeWeaponAngle(Value * WheelControlMultiplier);
	}
}

void AOblivioCharacter::EnableAdjustFocus()
{
	bCanAdjustFocus = true;
}


void AOblivioCharacter::ToggleFlashlight()
{
	if (bFlashlightForcedOff) return;   // 암전 효과 진행 중 — 토글 차단
	if (Battery > 0.0f)
	{
		bIsFlashlightOn = !bIsFlashlightOn;
		UpdateFlashlightVisuals();
	}
}

void AOblivioCharacter::UpdateFlashlightVisuals()
{
	if (!IsValid(CurrentWeapon)) return;

	if (bIsFlashlightOn) {	//On
		CurrentWeapon->UseWeapon();
	}
	else {	//Off
		CurrentWeapon->StopWeapon();
	}
}


void AOblivioCharacter::UpdateFlashlightEmbedPullback(float DeltaSeconds)
{
	UWorld* const World = GetWorld();
	if (!World || DeltaSeconds <= 0.f)
	{
		return;
	}

	ULightAttackComponent* Lac =
		IsValid(CurrentWeapon) ? CurrentWeapon->FindComponentByClass<ULightAttackComponent>() : nullptr;

	const bool bWeaponOk =
		IsValid(CurrentWeapon) && CurrentWeapon->IsA(AFlashlight::StaticClass()) && Lac != nullptr
		&& Lac->bIsConcentrated && IsValid(Lac->GetSpotLightComp());

	const bool bFlashOn = bIsFlashlightOn && Battery > 0.0f && !bFlashlightForcedOff;

	const bool bWantPull =
		bFlashlightPullbackFromWallsEnabled && bWeaponOk && bFlashOn;

	const bool bWantAttenClamp = bFlashlightWallAttenuationClampEnabled && bWeaponOk && bFlashOn;

	if (!bWantPull && !bWantAttenClamp)
	{
		FlashlightWallPullbackSmoothed = FMath::FInterpTo(FlashlightWallPullbackSmoothed, 0.f, DeltaSeconds,
			FlashlightWallPullbackInterpSpeed);

		if (FlashlightSpotPullbackWeakKey.IsValid() && bHasFlashlightSpotPullbackBaseline)
		{
			if (USpotLightComponent* const Spot = FlashlightSpotPullbackWeakKey.Get())
			{
				if (USceneComponent* const Parent = Spot->GetAttachParent())
				{
					const FVector MoveWorld = -Spot->GetForwardVector().GetSafeNormal() * FlashlightWallPullbackSmoothed;
					const FVector PullDeltaLocal = Parent->GetComponentTransform().InverseTransformVectorNoScale(MoveWorld);
					Spot->SetRelativeLocation(FlashlightSpotBaselineRelative + PullDeltaLocal);
				}
				else
				{
					Spot->SetRelativeLocation(FlashlightSpotBaselineRelative);
				}
			}
		}

		if (FMath::IsNearlyZero(FlashlightWallPullbackSmoothed, 0.02f))
		{
			FlashlightSpotPullbackWeakKey.Reset();
			bHasFlashlightSpotPullbackBaseline = false;
			FlashlightSpotBaselineRelative = FVector::ZeroVector;
		}

		if (bFlashlightAttenuationClampWasApplied && Lac != nullptr && IsValid(Lac->GetSpotLightComp()))
		{
			const float RefUU = Lac->LightDistance * 10.f;
			FlashlightWallAttenuationSmoothedUU = FMath::FInterpTo(
				FlashlightWallAttenuationSmoothedUU, RefUU, DeltaSeconds, FlashlightWallAttenuationInterpSpeed);
			Lac->GetSpotLightComp()->SetAttenuationRadius(FlashlightWallAttenuationSmoothedUU);
			if (FMath::IsNearlyEqual(FlashlightWallAttenuationSmoothedUU, RefUU, 1.f))
			{
				bFlashlightAttenuationClampWasApplied = false;
			}
		}
		return;
	}

	USpotLightComponent* const ActiveSpot = Lac->GetSpotLightComp();

	const FVector TraceStart = GetActorLocation() + FVector(0.f, 0.f, FlashlightWallTraceHeightFromCenter);
	const FVector AimForward = GetActorForwardVector().GetSafeNormal();
	const FVector TraceEnd = TraceStart + AimForward * FlashlightWallTraceDistance;

	FHitResult Hit;
	FCollisionQueryParams QP(FName(TEXT("Flash_wall_embed")), /*bTraceComplex=*/false);
	QP.AddIgnoredActor(this);
	if (IsValid(CurrentWeapon))
	{
		QP.AddIgnoredActor(CurrentWeapon.Get());
	}

	const bool bHitWall =
		World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, QP) && Hit.bBlockingHit;

	const float ClearAlongFwd =
		bHitWall ? FMath::Max(Hit.Distance - FlashlightWallEmbedSafetyMargin, KINDA_SMALL_NUMBER)
				 : FlashlightWallTraceDistance;

	float PullTargetCm = 0.f;
	if (bWantPull)
	{
		if (!FlashlightSpotPullbackWeakKey.IsValid() || FlashlightSpotPullbackWeakKey.Get() != ActiveSpot)
		{
			FlashlightWallPullbackSmoothed = 0.f;
			FlashlightSpotPullbackWeakKey = ActiveSpot;
			FlashlightSpotBaselineRelative = ActiveSpot->GetRelativeLocation();
			bHasFlashlightSpotPullbackBaseline = true;
		}

		const FVector LampWorld = ActiveSpot->GetComponentLocation();
		const float StickAlongView = FVector::DotProduct(LampWorld - TraceStart, AimForward);
		PullTargetCm = FMath::Clamp(StickAlongView - ClearAlongFwd, 0.f, FlashlightWallEmbedMaxPullback);
	}

	if (bWantPull && bHasFlashlightSpotPullbackBaseline)
	{
		FlashlightWallPullbackSmoothed = FMath::FInterpTo(
			FlashlightWallPullbackSmoothed, PullTargetCm, DeltaSeconds, FlashlightWallPullbackInterpSpeed);

		const FVector MoveWorld = -ActiveSpot->GetForwardVector().GetSafeNormal() * FlashlightWallPullbackSmoothed;
		if (USceneComponent* const Parent = ActiveSpot->GetAttachParent())
		{
			const FVector PullDeltaLocal = Parent->GetComponentTransform().InverseTransformVectorNoScale(MoveWorld);
			ActiveSpot->SetRelativeLocation(FlashlightSpotBaselineRelative + PullDeltaLocal);
		}
		else
		{
			ActiveSpot->SetRelativeLocation(FlashlightSpotBaselineRelative);
		}
	}

	if (!bWantPull)
	{
		FlashlightWallPullbackSmoothed = FMath::FInterpTo(
			FlashlightWallPullbackSmoothed, 0.f, DeltaSeconds, FlashlightWallPullbackInterpSpeed);
		if (FlashlightSpotPullbackWeakKey.IsValid() && bHasFlashlightSpotPullbackBaseline)
		{
			if (USpotLightComponent* const Spot = FlashlightSpotPullbackWeakKey.Get())
			{
				if (USceneComponent* const Parent = Spot->GetAttachParent())
				{
					const FVector MoveWorld = -Spot->GetForwardVector().GetSafeNormal() * FlashlightWallPullbackSmoothed;
					const FVector PullDeltaLocal = Parent->GetComponentTransform().InverseTransformVectorNoScale(MoveWorld);
					Spot->SetRelativeLocation(FlashlightSpotBaselineRelative + PullDeltaLocal);
				}
				else
				{
					Spot->SetRelativeLocation(FlashlightSpotBaselineRelative);
				}
			}
		}
		if (FMath::IsNearlyZero(FlashlightWallPullbackSmoothed, 0.02f))
		{
			FlashlightSpotPullbackWeakKey.Reset();
			bHasFlashlightSpotPullbackBaseline = false;
			FlashlightSpotBaselineRelative = FVector::ZeroVector;
		}
	}

	if (bWantAttenClamp)
	{
		const float RefCapUU = Lac->LightDistance * 10.f;
		float TargetAttenuationUU = RefCapUU;
		if (bHitWall)
		{
			const float DistLampToImpact = FVector::Distance(ActiveSpot->GetComponentLocation(), Hit.ImpactPoint);
			TargetAttenuationUU = FMath::Clamp(
				DistLampToImpact - FlashlightWallAttenuationMarginUU,
				FlashlightWallAttenuationMinUU,
				RefCapUU);
		}

		if (!bFlashlightAttenuationClampWasApplied)
		{
			FlashlightWallAttenuationSmoothedUU = ActiveSpot->AttenuationRadius;
		}
		bFlashlightAttenuationClampWasApplied = true;

		FlashlightWallAttenuationSmoothedUU = FMath::FInterpTo(
			FlashlightWallAttenuationSmoothedUU,
			TargetAttenuationUU,
			DeltaSeconds,
			FlashlightWallAttenuationInterpSpeed);

		ActiveSpot->SetAttenuationRadius(FlashlightWallAttenuationSmoothedUU);
	}
	else if (bFlashlightAttenuationClampWasApplied && Lac != nullptr && IsValid(Lac->GetSpotLightComp()))
	{
		const float RefUU = Lac->LightDistance * 10.f;
		FlashlightWallAttenuationSmoothedUU = FMath::FInterpTo(
			FlashlightWallAttenuationSmoothedUU, RefUU, DeltaSeconds, FlashlightWallAttenuationInterpSpeed);
		Lac->GetSpotLightComp()->SetAttenuationRadius(FlashlightWallAttenuationSmoothedUU);
		if (FMath::IsNearlyEqual(FlashlightWallAttenuationSmoothedUU, RefUU, 1.f))
		{
			bFlashlightAttenuationClampWasApplied = false;
		}
	}
}


void AOblivioCharacter::ReloadBattery()
{
	if (bIsDead) return;

	if (Battery >= 100.0f)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, TEXT("Battery is already full."));
		return;
	}

	if (InventoryComponent && InventoryComponent->ConsumeItem(EItemType::Battery, 1))
	{
		Battery = 100.0f;

		if (!bIsFlashlightOn)
		{
			bIsFlashlightOn = true;
			UpdateFlashlightVisuals();
		}

		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, TEXT("Flashlight Recharged!"));

		// UGameplayStatics::PlaySound2D(GetWorld(), ReloadSound);
	}
	else
	{
		// 배터리 아이템이 없을 때의 경고 메시지
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("No Battery in Inventory!"));
	}
}

void AOblivioCharacter::BeginThrow(TSubclassOf<AThrowableWeapon> Weapon)
{
	if (!bIsThrowing) {
		UE_LOG(LogTemp, Warning, TEXT("AnimateThrow!"));
		PlayAnimMontage(ThrowMontage);
		PendingThrowClass = Weapon;
		bIsThrowing = true;
	}

}

void AOblivioCharacter::ThrowWeapon() {
	bIsThrowing = false;
	if (!IsValid(PendingThrowClass)) {
		UE_LOG(LogTemp, Warning, TEXT("PendingThrowClass invalid!"));
		return;
	}
	FActorSpawnParameters Params;
	Params.Owner = this;
	AThrowableWeapon* ThrowingWeapon = GetWorld()->SpawnActor<AThrowableWeapon>(
		PendingThrowClass,
		GetActorLocation(),
		FRotator::ZeroRotator,
		Params);
	FVector temp = GetAimingLocation();
	UE_LOG(LogTemp, Warning, TEXT("Throwing Weapon %s to %f %f!"), *ThrowingWeapon->GetName(), temp.X, temp.Y);
	if (ThrowingWeapon) ThrowingWeapon->StartThrow(GetAimingLocation());
}

FVector AOblivioCharacter::GetAimingLocation() {
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!IsValid(PC)) return FVector::ZeroVector;
	FHitResult HitResult;
	PC->GetHitResultUnderCursor(
		ECC_Visibility,
		false,
		HitResult
	);

	return HitResult.Location;
}

void AOblivioCharacter::FadeOutFlashbang()
{
	// 빛의 강도를 서서히 줄임
	FlashbangIntensity -= 2000.0f;

	if (FlashbangIntensity <= 0.0f)
	{
		FlashbangIntensity = 0.0f;
		FlashbangLight->SetIntensity(0.0f);
		FlashbangLight->SetVisibility(false);

		// 타이머 종료
		GetWorldTimerManager().ClearTimer(FlashbangTimerHandle);
	}
	else
	{
		FlashbangLight->SetIntensity(FlashbangIntensity);
	}
}

//========================
// 전투 and 피해
//========================

void AOblivioCharacter::ApplyHealth(float Damage)
{
	if (bIsDead || bCheatGodMode) return;

	// 체력을 차감하고 최소값을 0으로 유지
	CurrentHealth = FMath::Clamp(CurrentHealth - Damage, 0.0f, MaxHealth);

	if (Damage > 0.0f)
	{

		if (IsValid(HitCameraShakeClass))
		{
			if (APlayerController* PC = Cast<APlayerController>(GetController()))
			{
				PC->ClientStartCameraShake(HitCameraShakeClass);
			}
		}
	}
	//TakeDamage에서 호출하던 델리게이트 이동
	// 블루프린트나 UI 갱신을 위해 델리게이트 방송
	OnPlayerDamaged.Broadcast(Damage, CurrentHealth, MaxHealth);

	if (CurrentHealth <= 0.0f)
	{
		HandleDeath();
	}
}

float AOblivioCharacter::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	const float AppliedDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	ApplyHealth(AppliedDamage); // 통합된 데미지 처리 함수 호출

	return AppliedDamage;
}

void AOblivioCharacter::GenerateFootstep()
{
	if (GetWorld()->GetTimerManager().IsTimerActive(FootstepTimerHandle)) {
		return;
	}
	GetWorld()->GetTimerManager().SetTimer(FootstepTimerHandle, 0.2f, false);
	UE_LOG(LogTemp, Warning, TEXT("GenerateFootstep Called"));
	//발걸음 SFX 출력
	if (IsValid(FootstepSound)) {
		UGameplayStatics::PlaySound2D(GetWorld(), FootstepSound);
	}

	//추적용 소리 전파
	if (IsValid(SoundPropagationComp)) {
		SoundPropagationComp->PropagateSound();
	}
}

void AOblivioCharacter::HandleDeath()
{
	if (bIsDead) return;
	bIsDead = true;

	if (bIsFlashlightOn)
	{
		bIsFlashlightOn = false;
		UpdateFlashlightVisuals();
	}

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
			{
				Subsystem->ClearAllMappings(); // 점프, 이동 등 모든 입력 끊기
			}
		}
	}

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->bPauseAnims = true;

		MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

		MeshComp->SetCollisionProfileName(TEXT("Ragdoll"));
		MeshComp->SetSimulatePhysics(true);
	}
	AActor* FloodActor = UGameplayStatics::GetActorOfClass(GetWorld(), AFloodLevelActor::StaticClass());
	if (FloodActor)
	{
		Cast<AFloodLevelActor>(FloodActor)->StopFloodEffects();
	}
	if (AOblivioGameMode* GM = Cast<AOblivioGameMode>(UGameplayStatics::GetGameMode(GetWorld())))
	{
		GM->GameOver();
	}
}

bool AOblivioCharacter::IsAlive() const
{
	return (CurrentHealth > 0.0f && !bIsDead);
}

void AOblivioCharacter::ApplyCCSlow(float SpeedMultiplier, float Duration)
{
	// 슬로우 로직 구현
	if (Duration <= 0.0f || bIsDead) return;

	bIsSlowed = true;
	CurrentSlowMultiplier = FMath::Clamp(SpeedMultiplier, 0.0f, 1.0f);

	GetWorldTimerManager().SetTimer(
		SlowTimerHandle,
		[this]()
		{
			bIsSlowed = false;
			CurrentSlowMultiplier = 1.0f;
		},
		Duration, /*bLoop=*/false);
}

void AOblivioCharacter::ApplyCCStun(float Duration)
{
	// 스턴 로직 구현
	if (Duration <= 0.0f || bIsDead) return;

	bIsStunned = true;

	GetWorldTimerManager().SetTimer(
		StunTimerHandle,
		[this]()
		{
			bIsStunned = false;
		},
		Duration, /*bLoop=*/false);
}

void AOblivioCharacter::ApplyFlashlightBlackout(float Duration)
{
	if (Duration <= 0.0f) return;

	bFlashlightForcedOff = true;

	// 켜져 있으면 즉시 끄기
	if (bIsFlashlightOn)
	{
		bIsFlashlightOn = false;
		UpdateFlashlightVisuals();
	}

	// Duration 후 강제 해제
	GetWorldTimerManager().SetTimer(
		FlashlightBlackoutTimer,
		[this]()
		{
			bFlashlightForcedOff = false;
		},
		Duration, /*bLoop=*/false);
}

void AOblivioCharacter::ApplyMovementInversion(float Duration)
{
	if (Duration <= 0.0f) return;

	bMovementInverted = true;

	GetWorldTimerManager().SetTimer(
		MovementInversionTimer,
		[this]()
		{
			bMovementInverted = false;
		},
		Duration, /*bLoop=*/false);
}

//cheat
void AOblivioCharacter::CheatFreeCraft()
{
	bCheatFreeCraft = !bCheatFreeCraft;
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Green, FString::Printf(TEXT("Free Crafting (No Cost): %s"), bCheatFreeCraft ? TEXT("ON") : TEXT("OFF")));
}

void AOblivioCharacter::CheatInfiniteBattery()
{
	bCheatInfiniteBattery = !bCheatInfiniteBattery;
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Green, FString::Printf(TEXT("Infinite Battery: %s"), bCheatInfiniteBattery ? TEXT("ON") : TEXT("OFF")));
}

void AOblivioCharacter::CheatGodMode()
{
	bCheatGodMode = !bCheatGodMode;
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Green, FString::Printf(TEXT("God Mode (Invincible): %s"), bCheatGodMode ? TEXT("ON") : TEXT("OFF")));
}