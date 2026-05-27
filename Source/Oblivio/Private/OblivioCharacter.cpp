#include "OblivioCharacter.h"
#include "AIEnemy/StagingEnemy.h"
#include "AIEnemy/CabinetEnemy.h"
#include "Cinematic/StagingCinematicTypes.h"
#include "OblivioCharacterAnimInstance.h"
#include "OblivioGameInstance.h"
#include "OblivioGameMode.h"
#include "Notify/PlayerFootstep.h"
#include "Notify/PlayerThrow.h"
#include "OblivioComponents/SoundPropagationComponent.h"
#include "OblivioComponents/PlayerCombatComponent.h"
#include "OblivioComponents/LightAttackComponent.h"
#include "Weapon/Flashlight.h"
#include "Items/FlashlightPickupItem.h"
#include "UI/OblivioFlashlightPromptWidget.h"
#include "UI/OblivioCabinetMashWidget.h"
#include "EngineUtils.h"
#include "Items/OblivioItemBase.h"
#include "Items/OblivioInventoryComponent.h"
#include "Crafting/OblivioCrafting.h"
#include "DoorBase.h"
#include "BestiaryCollectible.h"

#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "Components/CapsuleComponent.h"
#include "Components/MeshComponent.h"
#include "LevelSequence.h"
#include "LevelSequencePlayer.h"
#include "LevelSequenceActor.h"
#include "MovieSceneSequencePlaybackSettings.h"
#include "Animation/AnimInstance.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/HitResult.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "DrawDebugHelpers.h"
#include "Animation/AnimInstance.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"
#include "UObject/UObjectIterator.h"

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

	/** 눈알 게이트 해제용으로 동일하게 인식할 조건(ItemID 또는 액터 태그). */
	static bool Obliv_ItemUnlocksMementoEyeGate(const AOblivioItemBase* Item)
	{
		if (!IsValid(Item))
		{
			return false;
		}
		static const FName Key(TEXT("MementoEye"));
		if (Item->ItemID == Key)
		{
			return true;
		}
		return Item->ActorHasTag(Key);
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

	GrabFirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("GrabFirstPersonCamera"));
	if (USkeletalMeshComponent* SkelMesh = GetMesh())
	{
		GrabFirstPersonCamera->SetupAttachment(SkelMesh);
	}
	else
	{
		GrabFirstPersonCamera->SetupAttachment(RootComponent);
	}
	GrabFirstPersonCamera->bUsePawnControlRotation = false;
	GrabFirstPersonCamera->bAutoActivate = false;

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

	// [추가] 체력 저하 사운드 전용 오디오 컴포넌트 생성
	LowHealthAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("LowHealthAudioComponent"));
	LowHealthAudioComponent->SetupAttachment(RootComponent);
	LowHealthAudioComponent->bAutoActivate = false;
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
	if (IsValid(FlashlightWeapon))
	{
		Params.AddIgnoredActor(FlashlightWeapon);
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
	//8층부터는 현재 스탯을 인스턴스에 저장된 것으로 대체
	if (UOblivioGameInstance* GI = Cast<UOblivioGameInstance>(GetGameInstance()))
	{
		GI->LoadSessionPersistence();

		if (GI->CurrentFloor < 9)
		{
			CurrentHealth = GI->CurrentHealth;
			Battery = GI->CurrentBattery;
			Hunger = GI->CurrentHunger;
			Thirst = GI->CurrentThirst;
		}
	}

	Super::BeginPlay();

#if OBLIVIO_STAGING_GRAB_CINEMATIC_ENABLED
	if (USkeletalMeshComponent* SkelMesh = GetMesh())
	{
		const UClass* AnimClass = SkelMesh->GetAnimInstance()
			? SkelMesh->GetAnimInstance()->GetClass()
			: SkelMesh->AnimClass.Get();
		if (AnimClass && !AnimClass->IsChildOf(UOblivioCharacterAnimInstance::StaticClass()))
		{
			const FString Msg = FString::Printf(
				TEXT("Player AnimInstance should be OblivioCharacterAnimInstance, got %s — ABP Parent Class 확인"),
				*GetNameSafe(AnimClass));
			UE_LOG(LogTemp, Warning, TEXT("[PlayerABP] %s"), *Msg);
			if (GEngine && bDebugPlayerCinematicAnim)
			{
				GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Red,
					FString::Printf(TEXT("[PlayerABP] %s"), *Msg));
			}
		}
		else if (bDebugPlayerCinematicAnim && GEngine && AnimClass)
		{
			GEngine->AddOnScreenDebugMessage(-1, 4.f, FColor::Green,
				FString::Printf(TEXT("[PlayerABP] AnimInstance OK: %s"), *GetNameSafe(AnimClass)));
		}
	}
#endif

	if (CameraBoom)
	{
		CameraBoom->bDoCollisionTest = !bWallOcclusionDisableSpringArmProbe;
	}

	if (GrabFirstPersonCamera)
	{
		GrabFirstPersonCamera->SetActive(false);
	}

	if (TopDownCamera)
	{
		TopDownCamera->SetActive(true);
	}

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StartCameraFade(1.0f, 0.0f, 1.5f, FLinearColor::Black, false, true);
		}
	}

	//시작시 무기 미리 장착
	FActorSpawnParameters Params;
	Params.Owner = this;

	bIsFlashlightOn = false;

	const UOblivioGameInstance* PersistGI = Cast<UOblivioGameInstance>(GetGameInstance());
	const bool bShouldRestoreFlashlight = PersistGI && PersistGI->bFlashlightAcquired;

	if (bEquipFlashlightOnBeginPlay && !bShouldRestoreFlashlight)
	{
		GrantFlashlight(true);
	}

	//섬광탄
	if (IsValid(FlashbangClass)) {
		FlashbangWeapon = AttachWeapon(FlashbangClass, FName("RightHandSocket"));
	}

	//조명탄
	if (IsValid(BottleClass)) {
		BottleWeapon = AttachWeapon(BottleClass, FName("RightHandSocket"));
	}

	//AnimNotify 델리게이트 장착
	OnPlayerFootstep.AddDynamic(this, &AOblivioCharacter::GenerateFootstep);
	OnPlayerThrow.AddDynamic(this, &AOblivioCharacter::ThrowWeapon);
	bIsThrowing = false;

	//기존 기본부착 손전등 off
	FlashlightComponent->SetVisibility(false);

	if (bShouldRestoreFlashlight)
	{
		RestorePersistedFlashlight();
	}
	else if (HasFlashlight())
	{
		UpdateFlashlightVisuals();
	}

	if (!IsFlashlightPromptFloorActive())
	{
		bFlashlightWorldPickupEnabled = false;
		bFlashlightTurnOnPromptActive = false;
		if (GetWorld())
		{
			GetWorldTimerManager().ClearTimer(FlashlightTurnOnPromptTimer);
		}
	}

	TryPlayOpeningLevelSequence();
	UpdateFlashlightPromptUI();

	AFlashlightPickupItem::DestroyAllInWorldIfFlashlightAlreadyAcquired(this);
	AStagingEnemy::DestroyAllDefeatedInWorld(this);

	if (GetWorld())
	{
		TWeakObjectPtr<AOblivioCharacter> WeakThis(this);
		GetWorldTimerManager().SetTimerForNextTick([WeakThis]()
		{
			if (WeakThis.IsValid())
			{
				AFlashlightPickupItem::DestroyAllInWorldIfFlashlightAlreadyAcquired(WeakThis.Get());
				AStagingEnemy::DestroyAllDefeatedInWorld(WeakThis.Get());
			}
		});
	}
}

void AOblivioCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(FlashlightTurnOnPromptTimer);
	}
	StopOpeningLevelSequencePlayback(true);
	ClearWallOcclusionOverlays();
	Super::EndPlay(EndPlayReason);
}

//==========================
// Tick and Status
//==========================

void AOblivioCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bOpeningLevelSequenceActive && IsLocallyControlled())
	{
		MaintainOpeningLevelSequenceUIHidden();
	}

	EnforceCabinetGrabWorldTransformLock();
	UpdateStatus(DeltaTime);
	UpdateWallOcclusionDither();
	RefreshWallOcclusionFadeMaterialInstances();

	if (IsValid(FlashlightWeapon)) {
		FlashlightWeapon->SetActorRotation(GetActorRotation());
	}
	UpdateFlashlightEmbedPullback(DeltaTime);
}

void AOblivioCharacter::UpdateStatus(float DeltaTime)
{
	if (bIsDead) return;

	if (Hunger >= 80.0f)
	{
		float ExchangeAmount = 5.0f * DeltaTime;
		Hunger -= ExchangeAmount;
		CurrentHealth = FMath::Clamp(CurrentHealth + ExchangeAmount, 0.0f, MaxHealth);
	}
	else if (Hunger > 0.0f)
	{
		Hunger -= 0.3f * DeltaTime;
	}

	Hunger = FMath::Max(0.0f, Hunger);

	if (Hunger <= 0.0f)
	{
		CurrentHealth = FMath::Clamp(CurrentHealth - (1.0f * DeltaTime), 0.0f, MaxHealth);
	}

	if (bIsRunning)
	{
		// 달릴 때: 초당 10씩 감소
		Thirst -= 10.0f * DeltaTime;
		Thirst = FMath::Max(0.0f, Thirst);

		if (Thirst <= 0.0f)
		{
			// 스태미나가 다 닳으면 걷기 상태로 강제 전환
			StopRunning();
		}
	}
	else
	{
		// 달리지 않을 때: 초당 5씩 게이지가 서서히 다시 차오름
		Thirst = FMath::Clamp(Thirst + (5.0f * DeltaTime), 0.0f, 100.0f);
	}

	// 배터리 처리
	if (bIsFlashlightOn && Battery > 0.0f)
	{
		float FocusPenalty = FMath::Lerp(1.0f, 1.5f, CurrentFocusAlpha);
		if (!bCheatInfiniteBattery)
		{
			float OldBattery = Battery;
			Battery = FMath::Max(0.0f, Battery - (DeltaTime * BatteryDepletionRate * FocusPenalty));
			if (OldBattery != Battery)
			{
				OnBatteryChanged.Broadcast(Battery, 100.0f);
			}
		
		}

		if (Battery <= 0.0f)
		{
			bIsFlashlightOn = false;
			UpdateFlashlightVisuals();
			SyncFlashlightStateToGameInstance();
		}
	}

	OnPlayerDamaged.Broadcast(0.0f, CurrentHealth, MaxHealth);

	if (CurrentHealth <= 0.0f && !bIsDead)
	{
		HandleDeath();
	}


	bool bIsInWater = IsInWater();
	float WaterSpeedMultiplier = bIsInWater ? 0.5f : 1.0f;

	if (bIsStunned || bCinematicMovementLocked || bOpeningLevelSequenceActive)
	{
		// 스턴·연출 잠금 상태면 아예 움직이지 못함
		GetCharacterMovement()->MaxWalkSpeed = 0.0f;
	}
	else
	{
		float BaseSpeed = WalkSpeed;
		if (bIsCrouching) BaseSpeed = CrouchSpeed;
		else if (bIsRunning) BaseSpeed = RunSpeed;

		float FinalSpeed = BaseSpeed * WaterSpeedMultiplier;
		if (bIsSlowed) FinalSpeed *= CurrentSlowMultiplier;

		GetCharacterMovement()->MaxWalkSpeed = FinalSpeed;
	}

	// [사운드 추가] 체력 저하 시 과호흡/심박수 사운드 재생
	if (IsValid(LowHealthAudioComponent) && IsValid(LowHealthSound))
	{
		// 컴포넌트에 사운드가 세팅 안 되어있다면 한 번 세팅
		if (LowHealthAudioComponent->GetSound() != LowHealthSound)
		{
			LowHealthAudioComponent->SetSound(LowHealthSound);
		}

		// 체력이 임계점 이하이고, 살아있을 때
		if (CurrentHealth <= LowHealthThreshold && !bIsDead)
		{
			if (!LowHealthAudioComponent->IsPlaying())
			{
				LowHealthAudioComponent->Play();
			}
			LowHealthAudioComponent->SetPitchMultiplier(1.0f);
		}
		else
		{
			// 체력을 회복했거나 사망했다면 소리 정지
			if (LowHealthAudioComponent->IsPlaying())
			{
				LowHealthAudioComponent->Stop();
			}
		}
	}
}

//==========================
// 입력 and 상호작용
//==========================

void AOblivioCharacter::Move(const FVector2D& Value)
{
	if (bCinematicMovementLocked || bOpeningLevelSequenceActive)
	{
		return;
	}

	if (Controller != nullptr)
	{
		const float Dir = bMovementInverted ? -1.0f : 1.0f;
		AddMovementInput(FVector::ForwardVector, Value.Y * Dir);
		AddMovementInput(FVector::RightVector,   Value.X * Dir);
	}
}

void AOblivioCharacter::StartRunning() { bIsRunning = true; }
void AOblivioCharacter::StopRunning() { bIsRunning = false; }

void AOblivioCharacter::StartCrouching()
{
	bIsCrouching = true;
	bIsRunning = false;
}
void AOblivioCharacter::StopCrouching()
{
	bIsCrouching = false;
}

void AOblivioCharacter::ToggleInventory()
{
	bIsInventoryOpen = !bIsInventoryOpen;
	
	USoundBase* PlaySound = bIsInventoryOpen ? InventoryOpenSound : InventoryCloseSound;
	if (IsValid(PlaySound))
	{
		UGameplayStatics::PlaySound2D(GetWorld(), PlaySound);
	}

	OnInventoryToggle(bIsInventoryOpen);
}

void AOblivioCharacter::ToggleCrafting()
{
	if (CraftingComponent)
	{
		CraftingComponent->ToggleCraftingMode();
		bIsCraftingOpen = CraftingComponent->bIsCraftingModeActive;
		
		USoundBase* TargetSound = bIsCraftingOpen ? CraftingOpenSound : CraftingCloseSound;
		if (IsValid(TargetSound))
		{
			UGameplayStatics::PlaySound2D(GetWorld(), TargetSound);
		}

		OnCraftingUIToggle(bIsCraftingOpen);
	}
}

void AOblivioCharacter::PlaceObstacle()
{
	if (IsValid(ObstaclePlaceSound))
	{
		// 설치한 위치(내 발밑)에서 소리 재생
		UGameplayStatics::PlaySoundAtLocation(this, ObstaclePlaceSound, GetActorLocation());
	}
}

void AOblivioCharacter::Interact()
{
	if (bOpeningLevelSequenceActive)
	{
		return;
	}

	if (TryHandleCabinetMashInput())
	{
		return;
	}

	// 상호작용할 대상(TargetActor) 하나만 확실하게 정하기
	AActor* TargetActor = nullptr;

	for (AOblivioItemBase* NearbyItem : NearbyItemsList)
	{
		if (IsValid(NearbyItem) && NearbyItem->ActorHasTag(FName(TEXT("Flashlight"))))
		{
			TargetActor = NearbyItem;
			break;
		}
	}

	if (!TargetActor && NearbyItemsList.Num() > 0)
	{
		auto It = NearbyItemsList.CreateIterator();
		if (It && IsValid(*It))
		{
			TargetActor = *It;
		}
	}

	if (!TargetActor)
	{
		FHitResult HitResult;
		FVector Start = GetActorLocation();
		FVector End = Start + (GetActorForwardVector() * InteractionDistance);
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(this);

		FCollisionShape SphereShape = FCollisionShape::MakeSphere(40.0f);
		bool bHit = GetWorld()->SweepSingleByChannel(HitResult, Start, End, FQuat::Identity, ECC_Visibility, SphereShape, Params);

		if (bHit)
		{
			TargetActor = HitResult.GetActor();
		}
	}
	if (!TargetActor) return;

	// 손전등 획득 — E키 상호작용 (액터 태그 "Flashlight")
	if (TargetActor->ActorHasTag(FName("Flashlight")))
	{
		if (HasFlashlight())
		{
			return;
		}

		GrantFlashlight(false);

		if (AOblivioItemBase* PickedItem = Cast<AOblivioItemBase>(TargetActor))
		{
			PickedItem->OnInteract(this);
			RemoveNearbyItem(PickedItem);
			PickedItem->Destroy();
		}
		else
		{
			TargetActor->Destroy();
		}
		return;
	}

	// 문(Door) 상호작용
	if (ADoorBase* HitDoor = Cast<ADoorBase>(TargetActor))
	{
		HitDoor->InteractDoor();
		return;
	}


	// 태그 기반 상호작용 (유품, 체크포인트)
	AOblivioGameMode* GM = Cast<AOblivioGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
	if (GM)
	{
		if (TargetActor->ActorHasTag("Memento"))
		{
			if (AOblivioItemBase* PickedItem = Cast<AOblivioItemBase>(TargetActor))
			{
				PickedItem->OnInteract(this);
				ApplyDutyReadMomentEffects(PickedItem);

				if (UOblivioGameInstance* GI = Cast<UOblivioGameInstance>(GetGameInstance()))
				{
					if (Obliv_ItemUnlocksMementoEyeGate(PickedItem))
					{
						GI->bMementoEyeCollected = true;
					}
				}

				RemoveNearbyItem(PickedItem);
			}

			GM->AddMemento();
			TargetActor->Destroy();
			return;
		}
		else if (TargetActor->ActorHasTag("RestArea"))
		{
			GM->RestInteraction();
			return;
		}
	}

	// ------ 도감 상호작용 로직 ------
	if (ABestiaryCollectible* BestiaryDoc = Cast<ABestiaryCollectible>(TargetActor))
	{
		// E키를 눌렀을 때만 해금 함수 실행
		BestiaryDoc->CollectDocument(this);
		return;
	}

	// 일반 아이템 (인벤토리 추가)
	if (AOblivioItemBase* PickedItem = Cast<AOblivioItemBase>(TargetActor))
	{
		PickedItem->OnInteract(this);

		if (InventoryComponent && InventoryComponent->AddItem(PickedItem))
		{
			if (UOblivioGameInstance* GI = Cast<UOblivioGameInstance>(GetGameInstance()))
			{
				if (Obliv_ItemUnlocksMementoEyeGate(PickedItem))
				{
					GI->bMementoEyeCollected = true;
				}
			}
			if (PickedItem->ActorHasTag("Key") && GM)
			{
				GM->CollectedKeys++;
			}
			//인벤토리 추가 UI
			OnItemAcquiredEvent.Broadcast(PickedItem->ItemName, PickedItem->ItemIcon);
			RemoveNearbyItem(PickedItem);
			PickedItem->Destroy();
			return;
		}
	}
}

void AOblivioCharacter::ApplyForcedWorldLookTowards(FVector WorldLookTarget, float DurationSeconds,
	float RotationInterpSpeed)
{
	if (!GetWorld())
	{
		return;
	}

	ForcedLookWorldTarget = WorldLookTarget;
	ForcedLookEndTimeSeconds =
		GetWorld()->GetTimeSeconds() + FMath::Max(0.05f, DurationSeconds);
	ForcedLookInterpSpeed = FMath::Max(1.f, RotationInterpSpeed);
	bForcedWorldLookActive = true;
}

bool AOblivioCharacter::TryConsumeForcedWorldLookRotation(FRotator& OutTargetRotWorld)
{
	if (!bForcedWorldLookActive || !GetWorld())
	{
		return false;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now >= ForcedLookEndTimeSeconds)
	{
		bForcedWorldLookActive = false;
		return false;
	}

	OutTargetRotWorld =
		UKismetMathLibrary::FindLookAtRotation(GetActorLocation(), ForcedLookWorldTarget);
	return true;
}

void AOblivioCharacter::ApplyDutyReadMomentEffects(const AOblivioItemBase* ItemSrc)
{
	if (!IsLocallyControlled() || !IsValid(ItemSrc) || !ItemSrc->bApplyDutyReadMomentOnPickup)
	{
		return;
	}

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (ItemSrc->bDutyReadPlayDynamicForceFeedback && ItemSrc->DutyReadForceFeedbackDuration > KINDA_SMALL_NUMBER)
		{
			(void)PC->PlayDynamicForceFeedback(FMath::Clamp(ItemSrc->DutyReadForceFeedbackIntensity, 0.f, 1.f),
				ItemSrc->DutyReadForceFeedbackDuration,
				true,
				false,
				true,
				false,
				EDynamicForceFeedbackAction::Start,
				0);
		}
	}

	if (IsValid(ItemSrc->DutyReadBangSound))
	{
		UGameplayStatics::PlaySound2D(this, ItemSrc->DutyReadBangSound);
	}

	if (!ItemSrc->bDutyReadForceLookTowardsObservation || !GetWorld())
	{
		return;
	}

	const AActor* Resolved = ItemSrc->DutyReadObservationLookActor.Get();
	if (!Resolved && !ItemSrc->DutyReadObservationActorTag.IsNone())
	{
		TArray<AActor*> TaggedActors;
		UGameplayStatics::GetAllActorsWithTag(GetWorld(), ItemSrc->DutyReadObservationActorTag,
			TaggedActors);
		if (TaggedActors.Num() > 0 && IsValid(TaggedActors[0]))
		{
			Resolved = TaggedActors[0];
		}
	}

	if (Resolved && IsValid(Resolved))
	{
		ApplyForcedWorldLookTowards(Resolved->GetActorLocation(), ItemSrc->DutyReadForcedLookDuration,
			ItemSrc->DutyReadForcedLookInterpSpeed);
	}
}
void AOblivioCharacter::AddNearbyItem(AOblivioItemBase* Item)
{
	if (!IsValid(Item) || ShouldIgnoreItemPickupOverlap())
	{
		return;
	}

	NearbyItemsList.Add(Item);
	CurrentNearbyItem = Item;

	if (IsLocallyControlled() && !bSuppressNearbyPickupUIDuringOpeningSequence)
	{
		OnNearbyItemChanged.Broadcast(Item);
	}
}

void AOblivioCharacter::RemoveNearbyItem(AOblivioItemBase* Item)
{
	NearbyItemsList.Remove(Item);

	if (CurrentNearbyItem == Item)
	{
		if (NearbyItemsList.Num() > 0)
		{
			auto It = NearbyItemsList.CreateConstIterator();
			CurrentNearbyItem = *It;
		}
		else
		{
			CurrentNearbyItem = nullptr;
		}
	}

	if (!IsLocallyControlled() || bSuppressNearbyPickupUIDuringOpeningSequence)
	{
		return;
	}

	if (NearbyItemsList.Num() > 0)
	{
		auto It = NearbyItemsList.CreateConstIterator();
		OnNearbyItemChanged.Broadcast(*It);
	}
	else
	{
		// 발밑에 더이상 아이템이 없으면 UI 정리
		OnNearbyItemChanged.Broadcast(nullptr);
	}
}

void AOblivioCharacter::TogglePause()
{
	bIsPauseOpen = !bIsPauseOpen;

	USoundBase* PlaySound = bIsPauseOpen ? PauseOpenSound : PauseCloseSound;
	if (IsValid(PlaySound))
	{
		UGameplayStatics::PlaySound2D(GetWorld(), PlaySound);
	}

	OnPauseToggle(bIsPauseOpen);
	UpdateFlashlightPromptUI();
}

//=====================
//무기 장비
//======================

void AOblivioCharacter::AdjustFocus(float Value)
{
	if (bCanAdjustFocus && HasFlashlight() && IsValid(FlashlightWeapon))
	{
		FlashlightWeapon->ChangeWeaponAngle(Value * WheelControlMultiplier);
	}
}

void AOblivioCharacter::EnableAdjustFocus()
{
	bCanAdjustFocus = true;
}


void AOblivioCharacter::ToggleFlashlight()
{
	if (bFlashlightForcedOff || !bFlashlightAcquired) return;   // 암전 효과 진행 중 — 토글 차단
	if (Battery > 0.0f)
	{
		bIsFlashlightOn = !bIsFlashlightOn;
		UpdateFlashlightVisuals();
		SyncFlashlightStateToGameInstance();
	}
}

void AOblivioCharacter::SyncFlashlightStateToGameInstance() const
{
	if (UOblivioGameInstance* GI = Cast<UOblivioGameInstance>(GetGameInstance()))
	{
		GI->bFlashlightAcquired = bFlashlightAcquired;
		GI->bFlashlightOn = bIsFlashlightOn;
		if (bFlashlightAcquired)
		{
			GI->bFlashlightWorldPickupCollected = true;
		}
	}
}

void AOblivioCharacter::RestorePersistedFlashlight()
{
	if (bFlashlightAcquired || !IsValid(FlashlightClass))
	{
		return;
	}

	const UOblivioGameInstance* GI = Cast<UOblivioGameInstance>(GetGameInstance());
	if (!GI || !GI->bFlashlightAcquired)
	{
		return;
	}

	if (!IsValid(FlashlightWeapon))
	{
		FlashlightWeapon = AttachWeapon(FlashlightClass, FName("LeftHandSocket"));
	}

	if (!IsValid(FlashlightWeapon))
	{
		return;
	}

	bFlashlightAcquired = true;
	bFlashlightWorldPickupEnabled = false;
	bFlashlightTurnOnPromptActive = false;
	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(FlashlightTurnOnPromptTimer);
	}

	SetFlashlightWeaponVisible(true);
	bIsFlashlightOn = GI->bFlashlightOn && Battery > 0.f;
	if (bIsFlashlightOn)
	{
		FlashlightWeapon->UseWeapon();
	}
	else
	{
		FlashlightWeapon->StopWeapon();
	}

	SyncFlashlightStateToGameInstance();
	UpdateFlashlightPromptUI();
}

void AOblivioCharacter::GrantFlashlight(bool bTurnOn)
{
	if (!IsValid(FlashlightClass))
	{
		return;
	}

	if (!IsValid(FlashlightWeapon))
	{
		FlashlightWeapon = AttachWeapon(FlashlightClass, FName("LeftHandSocket"));
	}

	if (!IsValid(FlashlightWeapon))
	{
		return;
	}

	bFlashlightAcquired = true;
	bFlashlightWorldPickupEnabled = false;
	SetFlashlightWeaponVisible(true);
	bIsFlashlightOn = bTurnOn;
	UpdateFlashlightVisuals();

	bFlashlightTurnOnPromptActive = true;
	BeginFlashlightTurnOnPromptTimer();
	UpdateFlashlightPromptUI();

	AStagingEnemy::ActivateAllAfterFlashlightPickup(this, this);
	SyncFlashlightStateToGameInstance();
	AFlashlightPickupItem::DestroyAllInWorldIfFlashlightAlreadyAcquired(this);
}

void AOblivioCharacter::SetFlashlightWeaponVisible(bool bVisible)
{
	if (IsValid(FlashlightWeapon))
	{
		FlashlightWeapon->SetActorHiddenInGame(!bVisible);
	}
}

void AOblivioCharacter::EnableFlashlightWorldPickups()
{
	if (!GetWorld() || HasFlashlight() || !IsFlashlightPromptFloorActive())
	{
		return;
	}

	if (const UOblivioGameInstance* GI = Cast<UOblivioGameInstance>(GetGameInstance()))
	{
		if (GI->bFlashlightAcquired || GI->bFlashlightWorldPickupCollected)
		{
			AFlashlightPickupItem::DestroyAllInWorldIfFlashlightAlreadyAcquired(this);
			return;
		}
	}

	for (TActorIterator<AFlashlightPickupItem> It(GetWorld()); It; ++It)
	{
		It->SetPickupInteractable(true);
	}

	TArray<AActor*> TaggedPickups;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName(TEXT("Flashlight")), TaggedPickups);
	for (AActor* PickupActor : TaggedPickups)
	{
		if (!IsValid(PickupActor) || PickupActor->IsA(AFlashlightPickupItem::StaticClass()))
		{
			continue;
		}

		PickupActor->SetActorHiddenInGame(false);
		PickupActor->SetActorEnableCollision(true);
	}

	bFlashlightWorldPickupEnabled = true;
	UpdateFlashlightPromptUI();
}

void AOblivioCharacter::BeginFlashlightTurnOnPromptTimer()
{
	if (!GetWorld())
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(FlashlightTurnOnPromptTimer);
	GetWorldTimerManager().SetTimer(
		FlashlightTurnOnPromptTimer,
		this,
		&AOblivioCharacter::DismissFlashlightTurnOnPrompt,
		FMath::Max(0.1f, FlashlightTurnOnPromptDuration),
		false);
}

void AOblivioCharacter::DismissFlashlightTurnOnPrompt()
{
	bFlashlightTurnOnPromptActive = false;
	UpdateFlashlightPromptUI();
}

void AOblivioCharacter::EnsureFlashlightPromptWidget()
{
	if (FlashlightPromptWidget || !FlashlightPromptWidgetClass)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	FlashlightPromptWidget = CreateWidget<UOblivioFlashlightPromptWidget>(PC, FlashlightPromptWidgetClass);
}

bool AOblivioCharacter::IsFlashlightPromptFloorActive() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	static const FName Floor9LevelName(TEXT("L_Floor9_DoctorsLounge"));
	const FName CurrentLevelName = FName(*UGameplayStatics::GetCurrentLevelName(World, true));
	if (CurrentLevelName != Floor9LevelName)
	{
		return false;
	}

	if (const UOblivioGameInstance* GI = Cast<UOblivioGameInstance>(GetGameInstance()))
	{
		return GI->CurrentFloor == 9;
	}

	return true;
}

void AOblivioCharacter::UpdateFlashlightPromptUI()
{
	if (!IsLocallyControlled())
	{
		return;
	}

	if (!FlashlightPromptWidgetClass)
	{
		return;
	}

	if (bOpeningLevelSequenceActive || bIsPauseOpen)
	{
		if (FlashlightPromptWidget)
		{
			FlashlightPromptWidget->SetPromptPhase(EFlashlightPromptPhase::Hidden, true);
		}
		return;
	}

	if (!IsFlashlightPromptFloorActive())
	{
		if (FlashlightPromptWidget)
		{
			FlashlightPromptWidget->SetPromptPhase(EFlashlightPromptPhase::Hidden, true);
		}
		return;
	}

	EFlashlightPromptPhase DesiredPhase = EFlashlightPromptPhase::Hidden;

	if (bFlashlightTurnOnPromptActive)
	{
		DesiredPhase = EFlashlightPromptPhase::TurnOnWithF;
	}
	else if (bFlashlightWorldPickupEnabled && !HasFlashlight())
	{
		DesiredPhase = EFlashlightPromptPhase::AcquireWithE;
	}

	if (DesiredPhase == EFlashlightPromptPhase::Hidden)
	{
		if (FlashlightPromptWidget)
		{
			FlashlightPromptWidget->SetPromptPhase(EFlashlightPromptPhase::Hidden, false);
		}
		return;
	}

	EnsureFlashlightPromptWidget();
	if (!FlashlightPromptWidget)
	{
		return;
	}

	if (!FlashlightPromptWidget->IsInViewport())
	{
		FlashlightPromptWidget->AddToViewport(FlashlightPromptViewportZOrder);
	}

	FlashlightPromptWidget->SetPromptPhase(DesiredPhase);
}

void AOblivioCharacter::EnsureCabinetMashWidget()
{
	if (CabinetMashWidget || !CabinetMashWidgetClass)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	CabinetMashWidget = CreateWidget<UOblivioCabinetMashWidget>(PC, CabinetMashWidgetClass);
}

void AOblivioCharacter::UpdateCabinetMashUI(bool bActive, int32 CurrentPressCount, int32 RequiredPressCount)
{
	if (!IsLocallyControlled())
	{
		return;
	}

	if (!CabinetMashWidgetClass)
	{
		return;
	}

	if (!bActive)
	{
		if (CabinetMashWidget)
		{
			CabinetMashWidget->HideMashPrompt(false);
		}
		return;
	}

	EnsureCabinetMashWidget();
	if (!CabinetMashWidget)
	{
		return;
	}

	if (!CabinetMashWidget->IsInViewport())
	{
		CabinetMashWidget->AddToViewport(25);
		CabinetMashWidget->ShowMashPrompt(RequiredPressCount, false);
	}

	if (CurrentPressCount > 0)
	{
		CabinetMashWidget->SetMashProgress(CurrentPressCount, RequiredPressCount);
		CabinetMashWidget->PlayMashPressFeedback();
	}
}


void AOblivioCharacter::UpdateFlashlightVisuals()
{
	if (!bFlashlightAcquired || !IsValid(FlashlightWeapon)) return;

	if (IsValid(FlashlightClickSound))
	{
		UGameplayStatics::PlaySound2D(GetWorld(), FlashlightClickSound);
	}
	if (Battery <= 0.f)
	{
		bIsFlashlightOn = false;
	}
	if (bIsFlashlightOn) {	//On
		FlashlightWeapon->UseWeapon();
	}
	else {	//Off
		FlashlightWeapon->StopWeapon();
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
		IsValid(FlashlightWeapon) ? FlashlightWeapon->FindComponentByClass<ULightAttackComponent>() : nullptr;

	const bool bWeaponOk =
		IsValid(FlashlightWeapon) && FlashlightWeapon->IsA(AFlashlight::StaticClass()) && Lac != nullptr
		&& Lac->bIsConcentrated && IsValid(Lac->GetSpotLightComp());

	const bool bFlashOn = bIsFlashlightOn && Battery > 0.0f && !bFlashlightForcedOff;

	const bool bWantPull = bFlashlightPullbackFromWallsEnabled && bWeaponOk && bFlashOn;

	if (!bWantPull)
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
		return;
	}

	USpotLightComponent* const ActiveSpot = Lac->GetSpotLightComp();

	const FVector TraceStart = GetActorLocation() + FVector(0.f, 0.f, FlashlightWallTraceHeightFromCenter);
	const FVector AimForward = GetActorForwardVector().GetSafeNormal();
	const FVector TraceEnd = TraceStart + AimForward * FlashlightWallTraceDistance;

	FHitResult Hit;
	FCollisionQueryParams QP(FName(TEXT("Flash_wall_embed")), /*bTraceComplex=*/false);
	QP.AddIgnoredActor(this);
	if (IsValid(FlashlightWeapon))
	{
		QP.AddIgnoredActor(FlashlightWeapon.Get());
	}

	const bool bHitWall =
		World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, QP) && Hit.bBlockingHit;

	const float ClearAlongFwd =
		bHitWall ? FMath::Max(Hit.Distance - FlashlightWallEmbedSafetyMargin, KINDA_SMALL_NUMBER)
				 : FlashlightWallTraceDistance;

	if (!FlashlightSpotPullbackWeakKey.IsValid() || FlashlightSpotPullbackWeakKey.Get() != ActiveSpot)
	{
		FlashlightWallPullbackSmoothed = 0.f;
		FlashlightSpotPullbackWeakKey = ActiveSpot;
		FlashlightSpotBaselineRelative = ActiveSpot->GetRelativeLocation();
		bHasFlashlightSpotPullbackBaseline = true;
	}

	const FVector LampWorld = ActiveSpot->GetComponentLocation();
	const float StickAlongView = FVector::DotProduct(LampWorld - TraceStart, AimForward);
	const float PullTargetCm = FMath::Clamp(StickAlongView - ClearAlongFwd, 0.f, FlashlightWallEmbedMaxPullback);

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


void AOblivioCharacter::ReloadBattery()
{
	if (bIsDead || !HasFlashlight()) return;

	if (Battery >= 100.0f)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, TEXT("Battery is already full."));
		return;
	}

	if (InventoryComponent && InventoryComponent->ConsumeItem(EItemType::Battery, 1))
	{
		Battery = 100.0f;
		OnBatteryChanged.Broadcast(Battery, 100.0f);

		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, TEXT("Flashlight Recharged!"));

		// UGameplayStatics::PlaySound2D(GetWorld(), ReloadSound);
	}
	else
	{
		// 배터리 아이템이 없을 때의 경고 메시지
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("No Battery in Inventory!"));
	
		OnShowNoBatteryNotice();
	}
}

//무기 장착 함수
AWeaponBase* AOblivioCharacter::AttachWeapon(TSubclassOf<AWeaponBase> AttachingWeaponClass, FName AttachingSocket) {
	//무기 클래스 이상하면 거부
	if (!IsValid(AttachingWeaponClass)) {
		UE_LOG(LogTemp,Warning, TEXT("AttachingWeapon is not valid!"))
		return nullptr;
	}

	//무기 스폰 후 소켓에 장착
	FActorSpawnParameters Params;
	Params.Owner = this;
	AWeaponBase* NewWeapon = GetWorld()->SpawnActor<AWeaponBase>(AttachingWeaponClass, GetActorTransform(), Params);
	if (IsValid(NewWeapon)) {
		UE_LOG(LogTemp, Warning, TEXT("Attaching Weapon %s on %s"), *AttachingWeaponClass->GetName(), *AttachingSocket.ToString());
		NewWeapon->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, AttachingSocket);
	}
	return NewWeapon;
}

//섬광탄 시작 함수
void AOblivioCharacter::UseFlashbang()
{
	//다른 투척 호출중이면 실행 거부
	if (IsValid(PendingWeaponClass)) return;

	//배터리 있으면 실행
	if (InventoryComponent && InventoryComponent->HasItem("battery"))
	{
		UE_LOG(LogTemp, Warning, TEXT("UseFlashbang()!!"));
		bool IsWeaponReady = FlashbangWeapon->UseWeapon();
		if (IsWeaponReady) PendingWeaponClass = FlashbangWeapon;
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("Cannot use flashbang! no battery!"));
	}
	
}

//조명탄 시작 함수
void AOblivioCharacter::ThrowBottle()
{
	//다른 투척 호출중이면 실행 거부
	if (bIsDead || IsValid(PendingWeaponClass)) return;

	if (InventoryComponent && InventoryComponent->ConsumeItem(EItemType::Bottle, 1))
	{
		UE_LOG(LogTemp, Warning, TEXT("UseBottle()!!"));
		bool IsWeaponReady = BottleWeapon->UseWeapon();
		if (IsWeaponReady) PendingWeaponClass = BottleWeapon;
	}
	else
	{
		// 유리병이 없을 때의 피드백
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("No Bottle in Inventory!"));
	}
}

//AnimNotify로 타이밍에 맞게 투사체 스폰 호출
void AOblivioCharacter::ThrowWeapon() {
	//할당 이상하면 실행 거부
	if (!IsValid(PendingWeaponClass)) {
		UE_LOG(LogTemp, Warning, TEXT("No Weapon!!"));
		return;
	}

	//섬광탄
	if (PendingWeaponClass == FlashbangWeapon) {
		if (InventoryComponent && InventoryComponent->ConsumeItem(EItemType::Battery, 1))
		{
			UE_LOG(LogTemp, Warning, TEXT("Throwing flashbang!"));
			PendingWeaponClass->ExecuteWeaponAttack(GetAimingLocation());
		}
		else {
			UE_LOG(LogTemp, Warning, TEXT("Cannot spawn flashbang! no battery!"));
		}
	}
	//유리병
	else if (PendingWeaponClass == BottleWeapon) {
		UE_LOG(LogTemp, Warning, TEXT("Throwing Flare"));
		PendingWeaponClass->ExecuteWeaponAttack(GetAimingLocation());
	}


	PendingWeaponClass = nullptr;
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
	else {
		PlayHitAnim();
	}
}

float AOblivioCharacter::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	const float AppliedDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	ApplyHealth(AppliedDamage); // 통합된 데미지 처리 함수 호출

	return AppliedDamage;
}

void AOblivioCharacter::PlayHitAnim()
{
	UAnimInstance* Anim = GetMesh()->GetAnimInstance();

	if (Anim && !Anim->Montage_IsPlaying(HitMontage)) {
		PlayAnimMontage(HitMontage);
	}
	
}
bool AOblivioCharacter::IsInWater() const
{
	FVector Start = GetActorLocation() + FVector(0.0f, 0.0f, 100.0f);
	FVector End = GetActorLocation() - FVector(0.0f, 0.0f, 150.0f);

	FHitResult HitResult;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	bool bHit = GetWorld()->LineTraceSingleByChannel(
		HitResult, Start, End, ECC_Visibility, Params
	);

	if (bHit && HitResult.GetActor())
	{

		return HitResult.GetActor()->ActorHasTag(FName("Water"));
	}

	return false;
}

void AOblivioCharacter::GenerateFootstep()
{
	UE_LOG(LogTemp, Warning, TEXT("GenerateFootstep Called"));
	if (GetWorld()->GetTimerManager().IsTimerActive(FootstepTimerHandle)) {
		return;
	}
	GetWorld()->GetTimerManager().SetTimer(FootstepTimerHandle, 0.2f, false);

	GetWorld()->GetTimerManager().SetTimer(
		FootstepTimerHandle,
		FTimerDelegate::CreateLambda([]() {}),
		0.2f,
		false
	);

	bool bInWater = IsInWater();

	float SoundMultiplier = (bIsCrouching && !bInWater) ? 0.5f : 1.0f;

	USoundBase* SoundToPlay = bInWater ? WaterFootstepSound : FootstepSound;

	//발걸음 SFX 출력
	if (IsValid(SoundToPlay)) {
		UGameplayStatics::PlaySound2D(GetWorld(), SoundToPlay, SoundMultiplier);
	}

	//추적용 소리 전파
	if (IsValid(SoundPropagationComp)) {
		SoundPropagationComp->PropagateSound(SoundMultiplier);
	}
}

void AOblivioCharacter::HandleDeath()
{
	if (bIsDead) return;
	bIsDead = true;

	RestoreGameplayCamera();
	GetWorldTimerManager().ClearTimer(GrabCameraSwitchTimerHandle);

	if (IsValid(LowHealthAudioComponent) && LowHealthAudioComponent->IsPlaying())
	{
		LowHealthAudioComponent->Stop();
	}

	if (bIsFlashlightOn)
	{
		bIsFlashlightOn = false;
		UpdateFlashlightVisuals();
	}
	SyncFlashlightStateToGameInstance();

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
	OnPlayerDied.Broadcast();
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

void AOblivioCharacter::ActivateGrabFirstPersonCamera()
{
	if (!bEnableGrabFirstPersonCamera || bGrabFirstPersonCameraActive || !IsLocallyControlled())
	{
		return;
	}

	if (!GrabFirstPersonCamera || !TopDownCamera)
	{
		return;
	}

	ApplyGrabCameraSwitch(true);
}

void AOblivioCharacter::RestoreGameplayCamera()
{
	if (!bGrabFirstPersonCameraActive)
	{
		return;
	}

	ApplyGrabCameraSwitch(false);
}

void AOblivioCharacter::ApplyGrabCameraSwitch(bool bToFirstPerson)
{
	if (!GrabFirstPersonCamera || !TopDownCamera)
	{
		return;
	}

	auto PerformSwitch = [this, bToFirstPerson]()
	{
		if (bToFirstPerson)
		{
			TopDownCamera->SetActive(false);
			GrabFirstPersonCamera->SetActive(true);
			bGrabFirstPersonCameraActive = true;
		}
		else
		{
			GrabFirstPersonCamera->SetActive(false);
			TopDownCamera->SetActive(true);
			bGrabFirstPersonCameraActive = false;

			if (CameraBoom)
			{
				CameraBoom->bDoCollisionTest = !bWallOcclusionDisableSpringArmProbe;
			}
		}
	};

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !PC->IsLocalController() || GrabCameraSwitchBlendTime <= KINDA_SMALL_NUMBER)
	{
		PerformSwitch();
		return;
	}

	APlayerCameraManager* CameraManager = PC->PlayerCameraManager;
	if (!CameraManager)
	{
		PerformSwitch();
		return;
	}

	const float HalfBlend = GrabCameraSwitchBlendTime * 0.5f;
	GetWorldTimerManager().ClearTimer(GrabCameraSwitchTimerHandle);
	CameraManager->StartCameraFade(0.f, 1.f, HalfBlend, FLinearColor::Black, false, true);

	TWeakObjectPtr<AOblivioCharacter> WeakThis(this);
	GetWorldTimerManager().SetTimer(
		GrabCameraSwitchTimerHandle,
		[WeakThis, PerformSwitch, HalfBlend]()
		{
			if (!WeakThis.IsValid())
			{
				return;
			}

			PerformSwitch();

			if (APlayerController* LocalPC = Cast<APlayerController>(WeakThis->GetController()))
			{
				if (APlayerCameraManager* LocalCameraManager = LocalPC->PlayerCameraManager)
				{
					LocalCameraManager->StartCameraFade(1.f, 0.f, HalfBlend, FLinearColor::Black, false, true);
				}
			}
		},
		HalfBlend,
		false);
}

void AOblivioCharacter::RefreshCinematicMovementLock()
{
	bCinematicMovementLocked =
		PlayerCinematicState == EPlayerCinematicState::BeingGrabbed
		|| PlayerCinematicState == EPlayerCinematicState::Standoff
		|| PlayerCinematicState == EPlayerCinematicState::Pushing;
}

void AOblivioCharacter::BeginStagingCinematic(AStagingEnemy* StagingEnemy)
{
	LinkedStagingEnemy = StagingEnemy;
	SetPlayerCinematicState(EPlayerCinematicState::BeingGrabbed);
}

void AOblivioCharacter::EndStagingCinematic()
{
	UpdateCabinetMashUI(false, 0, 0);
	RestoreGameplayCamera();
	LinkedStagingEnemy = nullptr;
	SetPlayerCinematicState(EPlayerCinematicState::Released);
}

bool AOblivioCharacter::TryHandleCabinetMashInput()
{
	ACabinetEnemy* CabinetEnemy = Cast<ACabinetEnemy>(LinkedStagingEnemy.Get());

	if (!IsValid(CabinetEnemy) || !CabinetEnemy->IsMashWindowActive())
	{
		for (TActorIterator<ACabinetEnemy> It(GetWorld()); It; ++It)
		{
			ACabinetEnemy* const Candidate = *It;
			if (IsValid(Candidate)
				&& Candidate->GetLinkedPlayer() == this
				&& Candidate->IsMashWindowActive())
			{
				CabinetEnemy = Candidate;
				LinkedStagingEnemy = CabinetEnemy;
				break;
			}
		}
	}

	if (!IsValid(CabinetEnemy) || !CabinetEnemy->IsMashWindowActive())
	{
		return false;
	}

	CabinetEnemy->RegisterMashPress();
	return true;
}

bool AOblivioCharacter::IsPlayerCinematicAnimDebugEnabled() const
{
	return bDebugPlayerCinematicAnim;
}

void AOblivioCharacter::SetPlayerCinematicState(EPlayerCinematicState NewState)
{
	if (PlayerCinematicState == NewState)
	{
		return;
	}

	PlayerCinematicState = NewState;
	RefreshCinematicMovementLock();
	OnPlayerCinematicStateChanged.Broadcast(this, NewState);
}

void AOblivioCharacter::HandlePlayerCinematicNotify(EPlayerCinematicNotify NotifyEvent)
{
	switch (NotifyEvent)
	{
	case EPlayerCinematicNotify::EnterGrabbed:
		SetPlayerCinematicState(EPlayerCinematicState::BeingGrabbed);
		ActivateGrabFirstPersonCamera();
		break;
	case EPlayerCinematicNotify::EnterStandoff:
		SetPlayerCinematicState(EPlayerCinematicState::Standoff);
		break;
	case EPlayerCinematicNotify::ExecuteAutoPush:
		if (AStagingEnemy* StagingEnemy = LinkedStagingEnemy.Get())
		{
			StagingEnemy->ExecuteAutoPush();
		}
		break;
	case EPlayerCinematicNotify::PushSucceeded:
		RestoreGameplayCamera();
		SetPlayerCinematicState(EPlayerCinematicState::Pushing);
		break;
	case EPlayerCinematicNotify::ReleaseFromGrab:
		RestoreGameplayCamera();
		ReleaseFromStagingGrab();
		break;
	case EPlayerCinematicNotify::ForceFlashlightOn:
		break;
	case EPlayerCinematicNotify::RestoreControl:
		ReleaseFromStagingGrab();
		EndStagingCinematic();
		break;
	default:
		break;
	}

	OnPlayerCinematicNotify(NotifyEvent);
}

void AOblivioCharacter::ReleaseFromStagingGrab()
{
	bCinematicMovementLocked = false;
	RestoreGameplayCamera();
	if (PlayerCinematicState == EPlayerCinematicState::BeingGrabbed
		|| PlayerCinematicState == EPlayerCinematicState::Standoff)
	{
		SetPlayerCinematicState(EPlayerCinematicState::Released);
	}
}

void AOblivioCharacter::SetCabinetGrabWorldTransformLock(bool bEnabled, const FVector& WorldLocation, const FRotator& WorldRotation)
{
	if (bEnabled)
	{
		bCabinetGrabWorldTransformLockActive = true;
		CabinetGrabLockedWorldLocation = WorldLocation;
		CabinetGrabLockedWorldRotation = WorldRotation;

		if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
		{
			bCabinetGrabMovementTickWasEnabled = MoveComp->IsComponentTickEnabled();
			MoveComp->SetComponentTickEnabled(false);
			MoveComp->StopMovementImmediately();
			MoveComp->SetMovementMode(MOVE_None);
		}
	}
	else
	{
		bCabinetGrabWorldTransformLockActive = false;

		if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
		{
			MoveComp->SetComponentTickEnabled(bCabinetGrabMovementTickWasEnabled);
			if (IsAlive())
			{
				MoveComp->SetMovementMode(MOVE_Walking);
			}
		}
	}
}

void AOblivioCharacter::EnforceCabinetGrabWorldTransformLock()
{
	if (!bCabinetGrabWorldTransformLockActive)
	{
		return;
	}

	if (!CabinetGrabLockedWorldLocation.Equals(GetActorLocation(), 0.5f))
	{
		SetActorLocation(CabinetGrabLockedWorldLocation, false, nullptr, ETeleportType::TeleportPhysics);
	}

	if (!CabinetGrabLockedWorldRotation.Equals(GetActorRotation(), 0.5f))
	{
		SetActorRotation(CabinetGrabLockedWorldRotation);
	}

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
		MoveComp->Velocity = FVector::ZeroVector;
	}
}

void AOblivioCharacter::ForceFlashlightOnForCinematic()
{
	bFlashlightForcedOff = false;
	GetWorldTimerManager().ClearTimer(FlashlightBlackoutTimer);
}

void AOblivioCharacter::EnsureGameplayAnimClass()
{
	USkeletalMeshComponent* SkelMesh = GetMesh();
	if (!SkelMesh)
	{
		return;
	}

	if (SkelMesh->GetAnimClass() != nullptr)
	{
		return;
	}

	TSubclassOf<UAnimInstance> ClassToRestore = DefaultGameplayAnimClass;
	if (!ClassToRestore)
	{
		ClassToRestore = SkelMesh->AnimClass;
	}

	if (!ClassToRestore)
	{
		return;
	}

	SkelMesh->SetAnimInstanceClass(ClassToRestore);
	SkelMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	SkelMesh->InitAnim(true);
}

void AOblivioCharacter::PrepareForLevelSequence()
{
	USkeletalMeshComponent* SkelMesh = GetMesh();
	if (!SkelMesh || bAnimClassCachedForLevelSequence)
	{
		return;
	}

	TSubclassOf<UAnimInstance> AnimClassToCache = SkelMesh->GetAnimClass();
	if (!AnimClassToCache)
	{
		AnimClassToCache = DefaultGameplayAnimClass;
	}

	if (!AnimClassToCache)
	{
		return;
	}

	CachedAnimClassForLevelSequence = AnimClassToCache;
	bAnimClassCachedForLevelSequence = true;
	SkelMesh->SetAnimInstanceClass(nullptr);
}

void AOblivioCharacter::RestoreAfterLevelSequence()
{
	if (!bAnimClassCachedForLevelSequence)
	{
		EnsureGameplayAnimClass();
		return;
	}

	USkeletalMeshComponent* SkelMesh = GetMesh();
	if (!SkelMesh)
	{
		bAnimClassCachedForLevelSequence = false;
		CachedAnimClassForLevelSequence = nullptr;
		return;
	}

	TSubclassOf<UAnimInstance> ClassToRestore = CachedAnimClassForLevelSequence;
	if (!ClassToRestore)
	{
		ClassToRestore = DefaultGameplayAnimClass;
	}

	bAnimClassCachedForLevelSequence = false;
	CachedAnimClassForLevelSequence = nullptr;

	if (!ClassToRestore)
	{
		return;
	}

	SkelMesh->SetAnimInstanceClass(ClassToRestore);
	SkelMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	SkelMesh->InitAnim(true);
}

bool AOblivioCharacter::ShouldIgnoreItemPickupOverlap() const
{
	return bOpeningLevelSequenceActive || bCinematicMovementLocked;
}

void AOblivioCharacter::ClearNearbyPickupItems()
{
	NearbyItemsList.Empty();
	CurrentNearbyItem = nullptr;

	if (IsLocallyControlled())
	{
		OnNearbyItemChanged.Broadcast(nullptr);
	}
}

void AOblivioCharacter::SetWorldItemPickupCollisionsEnabled(bool bEnabled)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<AOblivioItemBase> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			It->SetPickupCollisionEnabled(bEnabled);
		}
	}
}

void AOblivioCharacter::ResolvePlayerHUDWidget()
{
	if (PlayerHUDWidget)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	for (TObjectIterator<UUserWidget> It; It; ++It)
	{
		UUserWidget* Widget = *It;
		if (!IsValid(Widget) || !Widget->IsInViewport())
		{
			continue;
		}

		if (Widget->GetOwningPlayer() != PC)
		{
			continue;
		}

		if (PlayerHUDWidgetClass && Widget->IsA(PlayerHUDWidgetClass))
		{
			PlayerHUDWidget = Widget;
			return;
		}

		const FString WidgetClassName = Widget->GetClass()->GetName();
		if (WidgetClassName.Contains(TEXT("WBP_PlayerHUD")))
		{
			PlayerHUDWidget = Widget;
			return;
		}
	}
}

void AOblivioCharacter::ApplyOpeningLevelSequenceUIVisibility(bool bUIVisible, bool bNotifyBlueprintEvent)
{
	if (!IsLocallyControlled())
	{
		return;
	}

	ResolvePlayerHUDWidget();
	if (PlayerHUDWidget)
	{
		PlayerHUDWidget->SetVisibility(
			bUIVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	UpdateFlashlightPromptUI();
	UpdateCabinetMashUI(false);

	bSuppressNearbyPickupUIDuringOpeningSequence = !bUIVisible;

	if (!bUIVisible)
	{
		ClearNearbyPickupItems();
		SetWorldItemPickupCollisionsEnabled(false);

		if (!bOpeningSequenceUIHidden)
		{
			bOpeningSequenceUIHidden = true;
		}
	}
	else
	{
		bOpeningSequenceUIHidden = false;
		SetWorldItemPickupCollisionsEnabled(true);

		if (IsValid(CurrentNearbyItem))
		{
			OnNearbyItemChanged.Broadcast(CurrentNearbyItem);
		}
		else if (NearbyItemsList.Num() > 0)
		{
			auto It = NearbyItemsList.CreateConstIterator();
			OnNearbyItemChanged.Broadcast(*It);
		}
		else
		{
			OnNearbyItemChanged.Broadcast(nullptr);
		}
	}

	if (bNotifyBlueprintEvent)
	{
		OnOpeningLevelSequenceUIChanged(bUIVisible);
	}
}

void AOblivioCharacter::MaintainOpeningLevelSequenceUIHidden()
{
	ResolvePlayerHUDWidget();
	if (PlayerHUDWidget)
	{
		PlayerHUDWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	UpdateFlashlightPromptUI();
	UpdateCabinetMashUI(false);
}

void AOblivioCharacter::BeginOpeningLevelSequenceControl()
{
	if (bOpeningLevelSequenceActive)
	{
		return;
	}

	bOpeningLevelSequenceActive = true;
	bOpeningSequenceUIHidden = false;

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		CachedMovementModeForLevelSequence = MoveComp->MovementMode;
		MoveComp->StopMovementImmediately();
		MoveComp->DisableMovement();
	}

	ApplyOpeningLevelSequenceUIVisibility(false);
}

void AOblivioCharacter::EndOpeningLevelSequenceControl()
{
	if (!bOpeningLevelSequenceActive)
	{
		return;
	}

	bOpeningLevelSequenceActive = false;

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->SetMovementMode(CachedMovementModeForLevelSequence);
	}

	ApplyOpeningLevelSequenceUIVisibility(true);
}

void AOblivioCharacter::TryPlayOpeningLevelSequence()
{
	if (!IsLocallyControlled())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UOblivioGameInstance* GI = Cast<UOblivioGameInstance>(GetGameInstance());
	if (!GI)
	{
		return;
	}

	ULevelSequence* Sequence = GI->ResolveOpeningLevelSequence(World);
	if (!Sequence)
	{
		EnableFlashlightWorldPickups();
		return;
	}

	if (GI->HasPlayedOpeningLevelSequence(World))
	{
		UE_LOG(LogTemp, Log, TEXT("TryPlayOpeningLevelSequence: skipped — already played on %s"),
			*UGameplayStatics::GetCurrentLevelName(World, true));
		EnableFlashlightWorldPickups();
		return;
	}

	PrepareForLevelSequence();
	AStagingEnemy::PrepareAllForLevelSequence(this);
	BeginOpeningLevelSequenceControl();

	FMovieSceneSequencePlaybackSettings PlaybackSettings;
	ALevelSequenceActor* SequenceActor = nullptr;
	ULevelSequencePlayer* SequencePlayer = ULevelSequencePlayer::CreateLevelSequencePlayer(
		World,
		Sequence,
		PlaybackSettings,
		SequenceActor);

	if (!SequencePlayer)
	{
		EndOpeningLevelSequenceControl();
		AStagingEnemy::RestoreAllAfterLevelSequenceAbort(this);
		RestoreAfterLevelSequence();
		EnableFlashlightWorldPickups();
		UE_LOG(LogTemp, Warning, TEXT("TryPlayOpeningLevelSequence: failed to create player for %s"), *GetNameSafe(Sequence));
		return;
	}

	ActiveOpeningLevelSequencePlayer = SequencePlayer;
	ActiveOpeningLevelSequenceActor = SequenceActor;
	SequencePlayer->OnNativeFinished.BindUObject(this, &AOblivioCharacter::HandleOpeningLevelSequenceFinished);
	GI->MarkOpeningLevelSequencePlayed(World);
	SequencePlayer->Play();
}

void AOblivioCharacter::HandleOpeningLevelSequenceFinished()
{
	UE_LOG(LogTemp, Log, TEXT("TryPlayOpeningLevelSequence: OnNativeFinished"));

	ReleaseOpeningLevelSequencePlayer();
	EndOpeningLevelSequenceControl();
	AStagingEnemy::RestoreAllAfterLevelSequenceFinished(this);
	RestoreAfterLevelSequence();
	EnableFlashlightWorldPickups();

	UE_LOG(LogTemp, Log, TEXT("TryPlayOpeningLevelSequence: finished — player control restored"));
}

void AOblivioCharacter::ReleaseOpeningLevelSequencePlayer()
{
	if (ULevelSequencePlayer* Player = ActiveOpeningLevelSequencePlayer.Get())
	{
		Player->OnNativeFinished.Unbind();
		if (Player->IsPlaying() || Player->IsPaused())
		{
			Player->Stop();
		}
	}

	ActiveOpeningLevelSequencePlayer.Reset();

	if (ALevelSequenceActor* SequenceActor = ActiveOpeningLevelSequenceActor.Get())
	{
		SequenceActor->Destroy();
	}

	ActiveOpeningLevelSequenceActor.Reset();
}

void AOblivioCharacter::StopOpeningLevelSequencePlayback(bool bRestoreAnim)
{
	ReleaseOpeningLevelSequencePlayer();

	if (!bRestoreAnim)
	{
		return;
	}

	EndOpeningLevelSequenceControl();
	AStagingEnemy::RestoreAllAfterLevelSequenceAbort(this);
	RestoreAfterLevelSequence();
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

void AOblivioCharacter::CheatGrantFlashlight()
{
	if (HasFlashlight())
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Yellow, TEXT("Flashlight already acquired."));
		}
		return;
	}

	GrantFlashlight(true);

	bFlashlightTurnOnPromptActive = false;
	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(FlashlightTurnOnPromptTimer);
	}
	UpdateFlashlightPromptUI();

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Green, TEXT("Flashlight granted (cheat)."));
	}
}