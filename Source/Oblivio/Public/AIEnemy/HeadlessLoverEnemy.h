#pragma once

// =============================================================================
// AHeadlessLoverEnemy — "머리없는 연인"
//
// 특성:
//   - 빛 면역(사망): ULightDamageType 으로 Die() 가 호출되면 체력 1 유지. 그 외 데미지는 사망 가능.
//   - 암전 능력: 3분(BlackoutCooldown)마다 플레이어 후레시를 5초(BlackoutDuration) 강제 OFF.
//     플레이어 거리·범위 무관하게 발동한다.
//   - 공포 공격: Anim Notify 의 Commit 타이밍에 플레이어 이동 방향을 MovementInversionDuration초 반전.
// =============================================================================

#include "CoreMinimal.h"
#include "AIEnemy/EnemyBase.h"
#include "HeadlessLoverEnemy.generated.h"

UCLASS(Blueprintable)
class OBLIVIO_API AHeadlessLoverEnemy : public AEnemyBase
{
	GENERATED_BODY()

public:
	AHeadlessLoverEnemy();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** OnLightHit 경직(CC) — 베이스 위임. */
	virtual void OnLightHit(float Intensity, float Duration) override;

	/** ULightDamageType 으로는 사망하지 않음(체력 1 유지). 그 외 데미지는 Super::Die(). */
	virtual void Die() override;

	/** 헤드리스 공포 타격: 노티 타이밍에 이동 반전 + 브로드캐스트. */
	virtual void CommitAttackFromAnimNotify(AActor* OptionalTargetOverride = nullptr) override;

	/** 소리에 반응하지 않는다. */
	virtual bool IsSoundInvestigationEnabled() const override { return false; }

	// === 암전 능력 ===

	/** 암전 지속 시간(초). 후레시가 꺼진 채로 유지되는 시간. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|HeadlessLover|Blackout", meta = (ClampMin = "0.1"))
	float BlackoutDuration = 5.0f;

	/** 암전 발동 주기(초). 기본 180초(3분). 플레이어 범위 무관. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|HeadlessLover|Blackout", meta = (ClampMin = "1.0"))
	float BlackoutCooldown = 180.0f;

	// === 공포 공격 ===

	/** 근접 공격 히트 시 이동 반전 지속 시간(초). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|HeadlessLover|Fear", meta = (ClampMin = "0.1"))
	float MovementInversionDuration = 5.0f;

private:
	FTimerHandle BlackoutPulseTimer;

	/** BlackoutCooldown 마다 호출 — 플레이어 후레시 강제 OFF. */
	void TriggerBlackoutPulse();
};
