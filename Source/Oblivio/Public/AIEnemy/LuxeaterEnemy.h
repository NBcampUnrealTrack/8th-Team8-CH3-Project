#pragma once

#include "CoreMinimal.h"
#include "AIEnemy/EnemyBase.h"
#include "AIEnemy/LuxeaterLaserMeshProbeActor.h"
#include "LuxeaterEnemy.generated.h"

class UAudioComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class USoundBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FLuxeaterPhaseChangedSignature, class ALuxeaterEnemy*, Enemy, int32, OldPhase, int32, NewPhase);
/** 1페이즈에서 2페이즈로 바뀔 때만 호출(OnPhaseChanged와 함께 브로드캐스트). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLuxeaterEnteredPhaseTwoSignature, class ALuxeaterEnemy*, Enemy);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FLuxeaterLightAbsorbedSignature, class ALuxeaterEnemy*, Enemy, float, AbsorbedAmount, float, TotalAbsorbed);

/**
 * ALuxeaterEnemy - 6F boss, "빛을 먹는 자"
 * - 1페이즈 체력: 플레이어 배터리 100% 1개를 다 썼을 때(가만히 존 가정)·이론 라이트 총데미지에 배수를 곱해 산출(기본 x2).
 * - 빛 흡수: 10회까지, 회당 이동 +3%/스케일 +2%, 10초 CD·3초 채널(이동 정지 이후 채택 훅), 이후 미사용.
 * - 레이저: 30초 CD, 5초 차징(이동 정지)·투명 프로젝타일 또는 폴백 라인 트레이스로 동적 장애물/플레이어 판정·임팩트 나이아가라.
 * - 페이즈: 체력 50% 이하 시 2페이즈 등은 기존과 동일.
 * - 외부 전투 단일 진실 원천: NotifyBossHealthChanged로 미러링 시 자동 계산 체력은 적용하지 않음.
 */
UCLASS(Blueprintable)
class OBLIVIO_API ALuxeaterEnemy : public AEnemyBase
{
	GENERATED_BODY()

public:
	ALuxeaterEnemy();

	virtual void ApplyCCSlow(float SpeedMultiplier, float Duration = 0.0f) override;
	virtual void ApplyCCStun(float Duration = 0.0f) override;

	virtual void OnLightHit(float Intensity, float Duration) override;

	virtual bool ShouldSuppressAILocomotion() const override;

	virtual void RefreshWalkSpeedFromSources() override;

	virtual bool IsObstacleAttackEnabled() const override { return false; }

	UFUNCTION(BlueprintPure, Category = "Enemy|Luxeater")
	int32 GetBossPhase() const { return BossPhase; }

	/** 디스크립트 흡수 스택 수(기존 API 호환 목적으로 float 라이크 느낌이었던 이름 유지 안 함 → 스택 단위 의미 확인용). */
	UFUNCTION(BlueprintPure, Category = "Enemy|Luxeater")
	int32 GetLightAbsorbStacks() const { return LightAbsorbStacks; }

	UFUNCTION(BlueprintPure, Category = "Enemy|Luxeater")
	float GetAbsorbedLight() const { return static_cast<float>(LightAbsorbStacks); }

	UFUNCTION(BlueprintCallable, Category = "Enemy|Luxeater")
	void NotifyBossHealthChanged(float NewCurrentHealth, float NewMaxHealth);

	/** 프로젝타일 레이저 표면 충돌 시(임팩트 나이아가라는 프로브가 이미 스폰). */
	void LaserResolveProjectileImpact(FHitResult const& Hit);

	/** 프로젝타일 미해결 시 라인 트레이스 규칙 + 임팩트 FX (소리 없음). */
	void LaserRunTraceFallback(FVector const& TraceOriginWorld);

	UPROPERTY(BlueprintAssignable, Category = "Enemy|Luxeater|Events")
	FLuxeaterPhaseChangedSignature OnPhaseChanged;

	UPROPERTY(BlueprintAssignable, Category = "Enemy|Luxeater|Events",
		meta = (ToolTip = "체력 비율이 PhaseTwoHealthPercentThreshold 이하가 되어 2페이즈로 들어갈 때 한 번 호출됩니다."))
	FLuxeaterEnteredPhaseTwoSignature OnEnteredPhaseTwo;

	UPROPERTY(BlueprintAssignable, Category = "Enemy|Luxeater|Events")
	FLuxeaterLightAbsorbedSignature OnLightAbsorbed;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void UpdateAttack() override;
	virtual bool HasValidAggroTarget() const override;

	void ApplyBuffFromAbsorbStacks();
	void UpdateHealthPhase();
	void TickBossAbilities(float DeltaSeconds);
	void TryComputeMaxHealthFromPlayerFlashlight();

	void BeginLightAbsorbChannel();
	void FinishLightAbsorbChannel();
	void TryScheduleLightAbsorbFromHit();

	bool TryBeginLaserCharge();
	void ExecuteLaserAttack();

	virtual void NotifyEnemyDamageApplied(float AppliedDamage) override;

	void LaserSpawnImpactNiagaraAt(FVector const LocationWorld, FVector const ImpactNormalWorld);

	/** 채널·차지 중 수평으로 플레이어/어그로 대상 바라보기(bImmediate면 한 프레임에 맞춤). */
	void FaceAggroOrPlayerYaw(float DeltaSeconds, float InterpSpeedDegPerSec, bool bImmediate);

	/** 레이저 눈/발사 시작점. 메쉬 소켓이 없으면 액터 위치 기준 오프셋을 사용한다. */
	UFUNCTION(BlueprintPure, Category = "Enemy|Luxeater|Laser")
	FVector GetLaserTraceOrigin() const;

	/** NotifyBossHealthChanged 경로에서는 자동 계산 체력을 덮어쓰지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Luxeater|Stats")
	bool bOverrideComputedMaxHealth = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Luxeater|Stats", meta = (ClampMin = "0.001"))
	float MaxHealthBatteryDamageMultiplier = 2.0f;

	/** 무기 폰 불일치 시 사용하는 손전등 데미지(틱당). 플래시 라이터 기본과 맞춤. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Luxeater|Stats", meta = (ClampMin = "0.001"))
	float ReferenceFlashDamage = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Luxeater|Stats", meta = (ClampMin = "0.001"))
	float ReferenceFlashAttackInterval = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Luxeater|Stats", meta = (ClampMin = "0.001"))
	float ReferenceBatteryDepletionRate = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Luxeater|LightAbsorb")
	int32 MaxLightAbsorbStacks = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Luxeater|LightAbsorb", meta = (ClampMin = "0.0"))
	float LightAbsorbCooldownSeconds = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Luxeater|LightAbsorb", meta = (ClampMin = "0.01"))
	float LightAbsorbCastSeconds = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Luxeater|LightAbsorb",
		meta = (ClampMin = "1.0", ToolTip = "흡수 채널 중 플레이어 바라보는 요 회전 속도(d/s). 레이저 차징과 동일 패턴."))
	float LightAbsorbFacePlayerYawInterpDegPerSec = 360.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Luxeater|LightAbsorb", meta = (ClampMin = "0.0"))
	float BonusMoveSpeedMultiplierPerAbsorbStack = 0.03f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Luxeater|LightAbsorb", meta = (ClampMin = "0.0"))
	float BonusScaleMultiplierPerAbsorbStack = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Luxeater|Laser", meta = (ClampMin = "0.0"))
	float LaserCooldownSeconds = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Luxeater|Laser", meta = (ClampMin = "0.01"))
	float LaserChargeSeconds = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Luxeater|Laser", meta = (ClampMin = "0.01"))
	float LaserPlayerHealthFractionPerHit = 1.0f;

	/** 레이저가 맞은 크래프팅 장애물 등(BP 장애물)·TakeDamage 처리용 한 방 분량 피해. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Luxeater|Laser")
	float LaserCraftingBurstDamage = 100000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Luxeater|Laser")
	FName LaserEyeSocketName = TEXT("head");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Luxeater|Laser")
	FVector LaserOriginOffsetNoSocket = FVector(0.f, 0.f, 80.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Luxeater|VFX|SFX")
	TObjectPtr<UNiagaraSystem> LightAbsorbNiagaraSystem;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Luxeater|VFX|SFX")
	TObjectPtr<UNiagaraComponent> LightAbsorbNiagara;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Luxeater|VFX|SFX")
	TObjectPtr<USoundBase> LightAbsorbSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Luxeater|VFX|SFX")
	TObjectPtr<UNiagaraSystem> LaserChargeNiagaraSystem;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Luxeater|VFX|SFX")
	TObjectPtr<UNiagaraComponent> LaserChargeNiagara;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Luxeater|VFX|SFX")
	TObjectPtr<USoundBase> LaserChargeSound;

	/** 레이저 투명 프로젝타일(BP). 미지정 시 폴백으로 라인 트레이스만. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Luxeater|VFX|SFX")
	TSubclassOf<ALuxeaterLaserMeshProbeActor> LaserProjectileProbeClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Luxeater|VFX|SFX", meta = (ClampMin = "100.0"))
	float LaserProjectileSpeedUU = 96000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Luxeater|VFX|SFX", meta = (ClampMin = "1.0"))
	float LaserProjectileProbeRadiusUU = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Luxeater|VFX|SFX", meta = (ClampMin = "0.0"))
	float LaserProjectileFallbackTimePad = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Luxeater|VFX|SFX")
	TObjectPtr<UNiagaraSystem> LaserProjectileImpactNiagaraSystem;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Luxeater|VFX|SFX")
	TObjectPtr<USoundBase> LaserFireSound;

	/** 채널/차지 시전이 끝난 뒤 잔여 SFX 볼륨을 줄이는 시간. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Luxeater|VFX|SFX", meta = (ClampMin = "0.05"))
	float LuxeaterChannelSfxFadeOutSeconds = 0.35f;

	/** 빛 관련 경직 로직 무시 목적(~즉시 경직 반복 회피). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Luxeater")
	float LuxuryLightExposureNeverStuns = 1.0e6f;

	/** true면 한 번 어그로 반경에 들어온 뒤 거리 무시하고 영구 추격(보스 기본 동작). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Luxeater|Aggro")
	bool bStickyAggroOnceTriggered = true;

	/** 스케일 보간 속도(FInterpTo). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Luxeater|Float", meta = (ClampMin = "0.01"))
	float ScaleInterpSpeed = 0.4f;

	/** 2페이즈 진입 체력 비율. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Luxeater|Phase", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float PhaseTwoHealthPercentThreshold = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Luxeater|Float", meta = (ClampMin = "0.0"))
	float FloatAmplitude = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Luxeater|Float", meta = (ClampMin = "0.1"))
	float FloatSpeed = 1.2f;

private:
	void StopLuxeaterChannelSfxForRestart(TObjectPtr<UAudioComponent>& Comp);
	void FadeOutLuxeaterChannelSfxOnCastEnd(TObjectPtr<UAudioComponent>& Comp);

	float BaseMoveSpeed = 0.0f;
	float BaseChaseMoveSpeed = 0.0f;
	FVector InitialScale = FVector::OneVector;
	float TargetScaleFromAbsorb = 1.0f;
	float CurrentScaleMultiplier = 1.0f;
	int32 LightAbsorbStacks = 0;
	int32 BossPhase = 1;
	bool bLaserChargeActive = false;
	double LaserChargeEndTimeSeconds = 0.0;
	double NextLaserAttackTimeSeconds = 0.0;
	bool bLightAbsorbChanneling = false;
	double LightAbsorbChannelEndTimeSeconds = 0.0;
	double NextLightAbsorbAvailableTimeSeconds = 0.0;
	bool bPendingLightAbsorbRequest = false;

	TObjectPtr<UAudioComponent> LightAbsorbCastSfxPlaying;
	TObjectPtr<UAudioComponent> LaserChargeCastSfxPlaying;

	bool bHealthMirroredFromExternalSystem = false;
	mutable bool bAggroLatched = false;
	float FloatBaseMeshRelativeZ = 0.0f;
	float FloatTime = 0.0f;
};
