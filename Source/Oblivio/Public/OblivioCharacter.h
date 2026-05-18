#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Items/OblivioItemBase.h"
#include "Weapon/WeaponBase.h"
#include "Weapon/ThrowableWeapon.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "OblivioComponents/CombatInterface.h"
#include "OblivioCharacter.generated.h"

// 피격 판정용 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FPlayerDamagedSignature, float, DamageAmount, float, CurrentHealth, float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNearbyItemChanged, class AOblivioItemBase*, NearbyItem);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPlayerAnimationEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayerDied);

//UI용 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemAcquired, const FText&, ItemName, class UTexture2D*, ItemIcon);

class UOblivioCrafting;
class UOblivioInventoryComponent;
class AWeaponBase;
class AThrowableWeapon;
class USoundPropagationComponent;
class UPlayerCombatComponent;
class UPrimitiveComponent;
class UMaterialInterface;
class USpotLightComponent;

UCLASS()
class OBLIVIO_API AOblivioCharacter : public ACharacter, public ICombatInterface
{
	GENERATED_BODY()

public:
	AOblivioCharacter();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;

	void ApplyWallOcclusionVisualToOccluder(UPrimitiveComponent* Prim);
	void RemoveWallOcclusionVisualFromOccluder(UPrimitiveComponent* Prim);
	void RefreshWallOcclusionFadeMaterialInstances();
	FVector GetWallOcclusionFocusWorldLocation() const;
	void UpdateWallOcclusionDither();
	void ClearWallOcclusionOverlays();
	bool ShouldTreatHitAsOccluderWall(const class UPrimitiveComponent* Component, FVector const& ImpactNormalWorld) const;
	/** Restrict 켰을 때 슬롯 베이스 머티리얼이 허용 목록과 같은지. */
	bool ShouldApplyWallOcclusionToPrimitive(UPrimitiveComponent const* Prim) const;
	/** 가림 디더 레이 시작 월드 위치(bWallOcclusionTraceStartUsesTopDownCameraWorldLocation에 따라 카메라 고정 또는 스프링암 논리점). */
	FVector GetWallOcclusionTraceStartWorld() const;
	/** Occluder 레이 끝점·로컬 MID 초점을 논리 카메라 쪽으로 살짝 당겨, 정면 벽을 볼 때 세그먼트가 벽을 스킵하지 않게 함. */
	FVector BiasWallOcclusionTraceEndTowardsTraceStartWorld(FVector SampleWorldHint) const;

public:
	//===========================
	//Components (컴포넌트)
	//==========================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Camera")
	class USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Camera")
	class UCameraComponent* TopDownCamera;

	/** true면 스프링암이 벽에 안 밀림(기본). 가리는 벽은 WallOcclusionOverlayMaterial 디더 오버레이로 처리. 바닥까지 걸리면 임계값·ExtraTraceLocals 조정. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components|Camera|Occlusion",
		meta = (ToolTip = "CameraBoom 프로브 충돌 비활성화. 카메라와 캐릭터 사이 레이에 걸린 스태틱 메시에 오버레이 재질 적용."))
	bool bWallOcclusionDisableSpringArmProbe = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components|Camera|Occlusion",
		meta = (ToolTip = "로컬 전용 뷰 타깃에게만 디더 레이 업데이트."))
	bool bWallOcclusionDitherEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Components|Camera|Occlusion",
		meta = (ToolTip =
			"Overlay: SetOverlayMaterial용. 불투명 베이스 위에서는 ‘뚫린 듯’ 반투명이 잘 안 보일 수 있음. Swap 모드: 슬럿 교체용 Translucent/Masked+디더 MI."))
	TObjectPtr<UMaterialInterface> WallOcclusionOverlayMaterial;

	/** 진짜로 얇게 비치려면 켠 뒤 WallOcclusionOverlayMaterial 에 벽 디폴트 재질을 본뜬 Transparent/Masked 페이드 MI 지정 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components|Camera|Occlusion")
	bool bWallOcclusionSwapMaterialInsteadOfOverlay = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components|Camera|Occlusion", meta = (ClampMin = "0"))
	int32 WallOcclusionMaterialSlotToSwap = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Components|Camera|Occlusion", meta = (ClampMin = "0.0"))
	float WallOcclusionUpdateIntervalSec = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Components|Camera|Occlusion", meta = (ClampMin = "0.01"))
	float WallOcclusionIgnoreBeyondBodyMarginUU = 30.f;

	/** 충돌 대상 채널(기본 가시 레이와 동일 계열로 벽 포함). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Components|Camera|Occlusion")
	TEnumAsByte<ECollisionChannel> WallOcclusionTraceChannel = ECC_Visibility;

	/**
	 * true면 디더 레이 시작점을 논리 스프링암 후보 선택 대신 TopDown 카메라의 실제 월드 좌표로 고정(+ ExtraOffset 합산).
	 * 줌만 바뀌고 시축 각도 고정 게임에서는 화면·디버그 선과 일치해 탐색이 안정적.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components|Camera|Occlusion")
	bool bWallOcclusionTraceStartUsesTopDownCameraWorldLocation = true;

	/** Occluder 레이 시작점에 더할 월드 오프셋(카모드 또는 블프 보정만 필요할 때). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Components|Camera|Occlusion")
	FVector WallOcclusionTraceStartWorldExtraOffset = FVector::ZeroVector;

	/** 바닥·천장으로 간주하여 오버레이 제외(abs(Z·Normal) 높음). 벽 중심 임계. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Components|Camera|Occlusion",
		meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "ImpactNormal 과 Up 과의 내적 상한. 그 위는 수평면으로 보고 무시."))
	float WallOcclusionFloorCeilingCosThreshold = 0.92f;

	/**
	 * 탑다운에서 카메라 레이가 난간·블록 등의 수평 윗면에 먼저 맞으면 법선이 거의 위를 향해 기본 필터에 걸려 오버레이가 안 붙을 수 있음.
	 * true면 바닥/천장 법선 제외를 하지 않음(타일 바닥 전체에 디더가 붙을 수 있으니 필요할 때만).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components|Camera|Occlusion",
		meta = (ToolTip = "true일 때 수평면(바닥/천장) 법선 필터를 끔. 탑다운 윗면 가림에 디더가 필요할 때 사용."))
	bool bWallOcclusionIgnoreFloorCeilingNormalFilter = false;

	/** 메시 로컬: 없으면 캡슐 샘플만 사용. 스켈 메시 있으면 해당 오프셋도 트레이스 끝으로 사용. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Components|Camera|Occlusion")
	FVector WallOcclusionSpineLocalOffsetFromMesh = FVector(0.f, 0.f, 70.f);

	/** 액터 변환 로컬 기준 추가 샘플(낮은 허벅지 등). 빈 목록 허용. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Components|Camera|Occlusion")
	TArray<FVector> WallOcclusionExtraTraceLocals;

	/**
	 * CapsuleRadius × 이 값 만큼 각 트레이스 끝 샘플을 GetWallOcclusionTraceStartWorld() 쪽으로 이동.
	 * 스켈/캡슐 샘플이 등 뒤쪽에만 있으면 카메라–등 구간이 벽을 안 끼고 빠져 디더가 안 걸릴 수 있음.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Components|Camera|Occlusion",
		meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float WallOcclusionTracePullTowardsTraceStartFracOfRadius = 0.45f;

	/** PIE/Development에서 논리 카메라에서 몸통 샘플로 쏘는 디더 레이 표시(Shipping 빌드에서는 실행하지 않음). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components|Camera|Occlusion|Debug")
	bool bWallOcclusionDebugDrawTraces = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components|Camera|Occlusion|Debug",
		meta = (ClampMin = "0.0", ToolTip = "0이면 엔진 기본(프레임마다 갱신용). 0 초과 시 해당 초만 라인 유지." ))
	float WallOcclusionDebugTraceLifeSec = 0.f;

	/** 실제 TopDownCamera와 논리 트레이스 시작점 보정 차이 헬퍼 선(Shipping에서는 실행하지 않음). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components|Camera|Occlusion|Debug")
	bool bWallOcclusionDebugDrawActualCameraGap = false;

	/**
	 * true면 피판정 메시마다 MID를 만들어 아래 FVector/Scalar 이름으로 중심·반경(·선 두께)을 매 틱 넣음.
	 * 재질에서 Absolute World Position ↔ 파라미터로 마스크해 “플레이어 근처만” 디더·반투명 처리(전면 벗기지 않음).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components|Camera|Occlusion|Localized")
	bool bWallOcclusionDriveLocalizedFadeWithMID = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Components|Camera|Occlusion|Localized")
	FName WallOcclusionMIDParam_FocusWorld = FName(TEXT("Occlusion_FocusWorld"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Components|Camera|Occlusion|Localized")
	FName WallOcclusionMIDParam_FocusRadiusUU = FName(TEXT("Occlusion_FocusRadiusUU"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Components|Camera|Occlusion|Localized")
	float WallOcclusionFocusRadiusUU = 140.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Components|Camera|Occlusion|Localized",
		meta = (ToolTip = "카메라~플레이어 선 근처 띠까지도 페이드(재질이 선 파라미터를 쓸 때)." ))
	bool bWallOcclusionMIDPassCameraToFocusLineParams = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Components|Camera|Occlusion|Localized")
	FName WallOcclusionMIDParam_LineStartWorld = FName(TEXT("Occlusion_LineStartWorld"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Components|Camera|Occlusion|Localized")
	FName WallOcclusionMIDParam_LineEndWorld = FName(TEXT("Occlusion_LineEndWorld"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Components|Camera|Occlusion|Localized")
	FName WallOcclusionMIDParam_LineHalfThicknessUU = FName(TEXT("Occlusion_LineHalfThicknessUU"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Components|Camera|Occlusion|Localized")
	float WallOcclusionFocusLineHalfThicknessUU = 40.f;

	/** true면 지정 슬롯 재질의 베이스가 WallOcclusionAllowedBaseMaterial 과 같을 때만 오클루전(오버레이·스왑) 적용. 예: Walls_1 전용 디더. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components|Camera|Occlusion|MaterialFilter",
		meta = (ToolTip = "벽 전부가 아니라 특정 머티리얼을 쓰는 메시에만 카메라 가림 디더를 씌울 때 사용."))
	bool bWallOcclusionRestrictToBaseMaterial = false;

	/** 비교 기준(보통 마스터 M Walls_1 또는 그 인스턴스). Restrict 켰을 때 필수. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Components|Camera|Occlusion|MaterialFilter",
		meta = (EditCondition = "bWallOcclusionRestrictToBaseMaterial"))
	TObjectPtr<UMaterialInterface> WallOcclusionAllowedBaseMaterial;

	/** 위 재질과 일치 여부를 볼 메시 슬롯(기본 0 = Element 0). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Components|Camera|Occlusion|MaterialFilter",
		meta = (EditCondition = "bWallOcclusionRestrictToBaseMaterial", ClampMin = "0"))
	int32 WallOcclusionMaterialMatchSlotIndex = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Light")
	class USpotLightComponent* FlashlightComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Light")
	class UPointLightComponent* FlashbangLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Crafting")
	TObjectPtr<UOblivioCrafting> CraftingComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Combat")
	TObjectPtr<USoundPropagationComponent> SoundPropagationComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Combat")
	TObjectPtr<UPlayerCombatComponent> CombatComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Inventory")
	TObjectPtr<UOblivioInventoryComponent> InventoryComponent;

	//===================================
	//Survival & Status (생존 및 상태)
	//==================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status|Health")
	float MaxHealth = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Status|Health")
	float CurrentHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status|Survival")
	float Hunger = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status|Survival")
	float Thirst = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status|Flashlight")
	float Battery = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status|Flashlight")
	float BatteryDepletionRate = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status|Flashlight")
	bool bIsFlashlightOn = true;

	/** 벽에 밀착 시 SpotLight 원점이 벽 안쪽으로 박혀 빛이 ‘뚫고’ 나오는 느낌을 줄이기 위해 라인 트레이스로 필요한 만큼 램프 위치를 발사축 역방향으로 당김. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status|Flashlight|WallEmbed")
	bool bFlashlightPullbackFromWallsEnabled = true;

	/** 라인 시작: 캡슐 중심(액터 위치 기준)에서 위(+Z) 오프셋(cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status|Flashlight|WallEmbed", meta = (ClampMin = "-120.0", ClampMax = "200.0"))
	float FlashlightWallTraceHeightFromCenter = 10.0f;

	/** 플레이어 전방으로 라인 최대 길이(cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status|Flashlight|WallEmbed", meta = (ClampMin = "40.0", ClampMax = "600.0"))
	float FlashlightWallTraceDistance = 200.0f;

	/** 히트 시 벽면에서 빛 시작을 더 띄우는 마진(cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status|Flashlight|WallEmbed", meta = (ClampMin = "0.0", ClampMax = "120.0"))
	float FlashlightWallEmbedSafetyMargin = 14.0f;

	/** 역방향으로 최대 당김(cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status|Flashlight|WallEmbed", meta = (ClampMin = "0.0", ClampMax = "120.0"))
	float FlashlightWallEmbedMaxPullback = 42.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status|Flashlight|WallEmbed", meta = (ClampMin = "1.0", ClampMax = "120.0"))
	float FlashlightWallPullbackInterpSpeed = 22.0f;

	/** 벽 히트 시 SpotLight 감쇠 반경을 램프~벽 거리 근처로 제한해 멀리까지 콘이 퍼지는 것을 줄임(디더 구멍 누설과는 별개). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status|Flashlight|WallEmbed")
	bool bFlashlightWallAttenuationClampEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status|Flashlight|WallEmbed", meta = (ClampMin = "0.0", ClampMax = "300.0"))
	float FlashlightWallAttenuationMarginUU = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status|Flashlight|WallEmbed", meta = (ClampMin = "10.0", ClampMax = "400.0"))
	float FlashlightWallAttenuationMinUU = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status|Flashlight|WallEmbed", meta = (ClampMin = "1.0", ClampMax = "80.0"))
	float FlashlightWallAttenuationInterpSpeed = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status|Upgrade")
	bool bCanAdjustFocus = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	int32 BatteryItemCount = 0;

	bool bIsDead = false;
	bool bIsRunning = false;
	bool bIsInventoryOpen = false;
	bool bIsCraftingOpen = false;
	bool bIsPauseOpen = false;
	float CurrentFocusAlpha = 0.5f;

	void UpdateStatus(float DeltaTime);
	void UpdateFlashlightVisuals();
	void UpdateFlashlightEmbedPullback(float DeltaSeconds);
	void ReloadBattery();

	UFUNCTION(BlueprintCallable, Category = "Status|Health")
	void HandleDeath();

	//=================================
	// Weapons & Items (무기 및 아이템)
	//==================================
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	TSubclassOf<AWeaponBase> FlashlightClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	TSubclassOf<AWeaponBase> FlashbangClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	TSubclassOf<AWeaponBase> FlareClass;

	TObjectPtr<AWeaponBase> FlashlightWeapon;
	TObjectPtr<AWeaponBase> FlashbangWeapon;
	TObjectPtr<AWeaponBase> FlareWeapon;

	//손전등 조절
	UFUNCTION(BlueprintCallable)
	void EnableAdjustFocus();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	float WheelControlMultiplier = 3.0f;

	//투척무기 관련
	FVector GetAimingLocation();
	FPlayerAnimationEvent OnPlayerThrow;
	TObjectPtr<AWeaponBase> PendingWeaponClass;
	FVector PendingThrowLocation;
	bool bIsThrowing;
	UFUNCTION()
	void ThrowWeapon();

	FTimerHandle FlashbangTimerHandle;
	float FlashbangIntensity = 0.0f;
	void FadeOutFlashbang();

	//피격 애니메이션
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Animation")
	UAnimMontage* HitMontage;
	void PlayHitAnim();

	// =====================================
	// Movement & Interaction 이동 및 상호작용
	// =====================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float WalkSpeed = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float RunSpeed = 400.0f;

	float CrouchSpeed = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	bool bIsCrouching = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	float InteractionDistance = 200.0f;

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnNearbyItemChanged OnNearbyItemChanged;

	UPROPERTY(BlueprintAssignable, Category = "Oblivio|Events")
	FOnItemAcquired OnItemAcquiredEvent;

	void SetNearbyItem(class AOblivioItemBase* Item);
	// 컨트롤러 바인딩 함수들
	void Move(const FVector2D& Value);
	void StartRunning();
	void StopRunning();
	void StartCrouching();
	void StopCrouching();
	void ToggleFlashlight();
	void UseFlashbang();
	void UseFlare();
	void AdjustFocus(float Value);
	
	UFUNCTION(BlueprintCallable, Category = "UI")
	void ToggleInventory();
	void ToggleCrafting();
	void PlaceObstacle();
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void Interact();
	
	UFUNCTION(BlueprintCallable, Category = "UI")
	void TogglePause();
	
	UFUNCTION(BlueprintImplementableEvent, Category = "UI")
	void OnCraftingUIToggle(bool bIsOpen);
	
	UFUNCTION(BlueprintImplementableEvent, Category = "UI")
	void OnInventoryToggle(bool bIsOpen);
	
	UFUNCTION(BlueprintImplementableEvent, Category = "UI")
	void OnPauseToggle(bool bIsOpen);

	//===============================
	// Combat Interface (전투 시스템)
	//===============================
	virtual void ApplyHealth(float Damage) override;
	virtual void ApplyCCSlow(float SpeedMultiplier, float Duration) override;
	virtual void ApplyCCStun(float Duration) override;
	virtual bool IsAlive() const override;

	bool bIsStunned = false;
	bool bIsSlowed = false;
	float CurrentSlowMultiplier = 1.0f;

	FTimerHandle StunTimerHandle;
	FTimerHandle SlowTimerHandle;

	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FPlayerDamagedSignature OnPlayerDamaged;

	UPROPERTY(EditDefaultsOnly, Category = "Feedback")
	TSubclassOf<class UCameraShakeBase> HitCameraShakeClass;

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnPlayerDied OnPlayerDied;

	//===============================
	// Fear Effects (공포 효과)
	//===============================
	/** 후레시를 Duration초 동안 강제 OFF한다. 그 동안 플레이어가 켤 수 없다. */
	UFUNCTION(BlueprintCallable, Category = "Fear")
	void ApplyFlashlightBlackout(float Duration);

	/** Move 입력 방향을 Duration초 동안 반전한다. */
	UFUNCTION(BlueprintCallable, Category = "Fear")
	void ApplyMovementInversion(float Duration);


	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound|Animation")
	TObjectPtr<USoundBase> FootstepSound;
	UFUNCTION(BlueprintCallable)
	void GenerateFootstep();
	FPlayerAnimationEvent OnPlayerFootstep;
	FTimerHandle FootstepTimerHandle;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound|Animation")
	TObjectPtr<USoundBase> WaterFootstepSound;

	//===============================
	//Cheat
	//=================================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cheats")
	bool bCheatFreeCraft = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cheats")
	bool bCheatInfiniteBattery = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cheats")
	bool bCheatGodMode = false;

	UFUNCTION(Exec, Category = "Cheats")
	void CheatFreeCraft();

	UFUNCTION(Exec, Category = "Cheats")
	void CheatInfiniteBattery();

	UFUNCTION(Exec, Category = "Cheats")
	void CheatGodMode();

	/** 2층 홍수 기믹: 현재 월드의 수위 (FloodLevelActor가 매 틱 업데이트함) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Flood")
	float CurrentWaterLevel = -1000.0f; // 초기값은 매우 낮게 설정

	/** 물속에 있는지 여부를 반환하는 함수 */
	UFUNCTION(BlueprintPure, Category = "Status|Flood")
	bool IsInWater() const;

	// ================= [사운드 에셋 변수] =================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	USoundBase* FlashlightClickSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	USoundBase* InventoryOpenSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	USoundBase* InventoryCloseSound;

	// 과호흡/심박수 사운드 (Looping 사운드 권장)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Health")
	USoundBase* LowHealthSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	USoundBase* CraftingOpenSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	USoundBase* CraftingCloseSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	USoundBase* ObstaclePlaceSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|UI")
	USoundBase* PauseOpenSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|UI")
	USoundBase* PauseCloseSound;

	// 체력이 이 수치 이하일 때 심장/과호흡 소리가 들림
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Health")
	float LowHealthThreshold = 30.0f;

protected:
	// 사운드를 지속적으로 재생하고 피치(속도)를 조절하기 위한 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Audio")
	UAudioComponent* LowHealthAudioComponent;

private:
	TSet<TWeakObjectPtr<UPrimitiveComponent>> WallOcclusionAppliedPrimitives;
	/** Swap 모드에서만 사용: 교체 전 슬롯 재질 */
	TMap<TWeakObjectPtr<UPrimitiveComponent>, TObjectPtr<UMaterialInterface>> WallOcclusionSavedSwapMaterials;

	TMap<TWeakObjectPtr<UPrimitiveComponent>, TObjectPtr<UMaterialInstanceDynamic>> WallOcclusionFadeMIDByOccluder;

	double WallOcclusionNextUpdateWorldTimeSeconds = 0.;

	UPROPERTY()
	class AOblivioItemBase* CurrentNearbyItem = nullptr;

	bool bFlashlightForcedOff = false;
	bool bMovementInverted    = false;
	FTimerHandle FlashlightBlackoutTimer;
	FTimerHandle MovementInversionTimer;

	/** Flashlight 무기 SpotLight 후퇴 보간 상태 */
	float FlashlightWallPullbackSmoothed = 0.f;
	FVector FlashlightSpotBaselineRelative = FVector::ZeroVector;
	TWeakObjectPtr<USpotLightComponent> FlashlightSpotPullbackWeakKey;
	bool bHasFlashlightSpotPullbackBaseline = false;

	float FlashlightWallAttenuationSmoothedUU = 0.f;
	bool bFlashlightAttenuationClampWasApplied = false;
};