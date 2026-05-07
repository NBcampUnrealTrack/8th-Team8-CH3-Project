#pragma once

#include "CoreMinimal.h"
#include "AIEnemy/EnemyBase.h"
#include "LuxeaterEnemy.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FLuxeaterPhaseChangedSignature, class ALuxeaterEnemy*, Enemy, int32, OldPhase, int32, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FLuxeaterLightAbsorbedSignature, class ALuxeaterEnemy*, Enemy, float, AbsorbedAmount, float, TotalAbsorbed);

/**
 * ALuxeaterEnemy - 6F boss, "빛을 먹는 자"
 * - 빛 피격 판단을 받으면 빛을 흡수해 이동속도·스케일만 증가 (체력 회복/상한 증가 없음)
 * - 페이즈는 체력 기준: 1페이즈 시작, 체력 50% 이하에서 2페이즈 (피격·동기화 후 자동 갱신)
 * - 외부 전투 모듈이 단일 진실 원천이면 NotifyBossHealthChanged로 미러링. 로컬 TakeDamage와 동시 사용 시 이중 차감 주의.
 * - CC 면역: ApplyCCSlow / ApplyCCStun 모두 무시(스킬·빛·컴포넌트 경로 공통).
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

	UFUNCTION(BlueprintPure, Category = "Enemy|Luxeater")
	int32 GetBossPhase() const { return BossPhase; }

	UFUNCTION(BlueprintPure, Category = "Enemy|Luxeater")
	float GetAbsorbedLight() const { return AbsorbedLight; }

	/**
	 * 전투 시스템이 보스 체력값을 갱신했을 때 호출.
	 * EnemyBase의 CurrentHealth/MaxHealth를 동기화하고 페이즈만 판단한다.
	 * (빛 흡수량과 무관, 빛 흡수로 체력은 오르지 않음)
	 */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Luxeater")
	void NotifyBossHealthChanged(float NewCurrentHealth, float NewMaxHealth);

	UPROPERTY(BlueprintAssignable, Category = "Enemy|Luxeater|Events")
	FLuxeaterPhaseChangedSignature OnPhaseChanged;

	UPROPERTY(BlueprintAssignable, Category = "Enemy|Luxeater|Events")
	FLuxeaterLightAbsorbedSignature OnLightAbsorbed;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void UpdateChase() override;
	virtual void UpdateAttack() override;
	virtual bool HasValidAggroTarget() const override;

	void ApplyLightEmpowerment();
	void UpdateHealthPhase();

	virtual void NotifyEnemyDamageApplied(float AppliedDamage) override;

	/** true면 한 번 어그로 반경에 들어온 뒤 거리 무시하고 영구 추격(보스 기본 동작). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Luxeater|Aggro")
	bool bStickyAggroOnceTriggered = true;

	/** 빛 흡수 누적량 1당 증가하는 이동속도(cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Luxeater|Light", meta = (ClampMin = "0.0"))
	float SpeedGainPerLight = 35.0f;

	/**
	 * 빛 흡수 누적량 1단위당 증가하는 목표 스케일.
	 * 실제 스케일은 ScaleInterpSpeed 속도로 목표치에 부드럽게 보간된다.
	 * 튜닝 메모: 0.04(너무 빠름) → 0.005(1단위당, 보간으로 점진 표현)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Luxeater|Light", meta = (ClampMin = "0.0"))
	float ScaleGainPerLight = 0.005f;

	/** 현재 스케일이 목표 스케일로 보간되는 속도(FInterpTo 속도값). 낮을수록 느리게 커짐. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Luxeater|Light", meta = (ClampMin = "0.01"))
	float ScaleInterpSpeed = 0.4f;

	/** 빛 흡수량으로 오를 수 있는 최대 추가 이동속도. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Luxeater|Light", meta = (ClampMin = "0.0"))
	float MaxLightSpeedBonus = 420.0f;

	/** 빛 흡수로 도달할 수 있는 최대 스케일 배율. (튜닝 메모: 1.6 → 1.12 → 1.3 으로 재조정 — 끝까지 자라야 +30%.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Luxeater|Light", meta = (ClampMin = "1.0"))
	float MaxLightScaleMultiplier = 1.3f;

	/** 2페이즈 진입 체력 비율. 기본 0.5 = 체력 절반 이하. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Luxeater|Phase", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float PhaseTwoHealthPercentThreshold = 0.5f;

private:
	float BaseMoveSpeed = 0.0f;
	float BaseChaseMoveSpeed = 0.0f;
	FVector InitialScale = FVector::OneVector;
	float AbsorbedLight = 0.0f;
	int32 BossPhase = 1;
	/** 빛 흡수량으로 계산된 목표 스케일 배율. 실제 스케일은 Tick에서 이 값으로 보간됨. */
	float TargetScaleMultiplier = 1.0f;
	/** 현재 적용 중인 스케일 배율(보간 중간값). */
	float CurrentScaleMultiplier = 1.0f;

	/** 플레이어가 한 번이라도 AggroRadius 안에 들어왔는지(이후엔 거리 무시). bStickyAggroOnceTriggered 와 함께 사용. */
	mutable bool bAggroLatched = false;
};
