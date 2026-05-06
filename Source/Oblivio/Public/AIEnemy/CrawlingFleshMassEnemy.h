#pragma once

// =============================================================================
// ACrawlingFleshMassEnemy (CRAWLING_FLESH_MASS) — swarm: Nav로 퍼진 뒤 한꺼번에 추격.
// 동기는 AEnemySpawner::OnWaveSpawnQueueEmptied. 즉사/빛/함정은 전투팀에서 TakeDamage 등 처리.
// =============================================================================

#include "CoreMinimal.h"
#include "AIEnemy/EnemyBase.h"
#include "CrawlingFleshMassEnemy.generated.h"

class AEnemySpawner;

UCLASS(Blueprintable)
class OBLIVIO_API ACrawlingFleshMassEnemy : public AEnemyBase
{
	GENERATED_BODY()

public:
	ACrawlingFleshMassEnemy();

	/** EnemySpawner가 스폰 직후 호출. 같은 스포너 웨이브 큐 종료 이벤트에 등록만 한다 */
	void InitializeSwarmMembership(AEnemySpawner* OwningSpawner);

	virtual void Tick(float DeltaSeconds) override;
	virtual void UpdateState() override;
	virtual void UpdateIdle(float DeltaSeconds) override;
	virtual void UpdateAttack() override;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	/** Nav 랜덤 목표의 원형 반경(cm). 스포너 위치를 중심으로 샘플 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|SwarmScatter", meta = (ClampMin = "64.0"))
	float ScatterRadius = 2200.f;

	/** 마지막 스폰 이후부터 이 시간(sec) 산포. 종료 후 설계본 Chase 어그로로 복귀해 일제 추격 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|SwarmScatter", meta = (ClampMin = "0.0"))
	float ScatterDuration = 6.f;

	/** 산포 중 새 무작향 목표 재선택 간격(sec) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|SwarmScatter", meta = (ClampMin = "0.2"))
	float ScatterRetargetInterval = 1.4f;

	/** 산포 중 일시적으로 줄이는 Aggro(cm). 플레이어가 극단적으로 가까울 때만 어그로 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|SwarmScatter", meta = (ClampMin = "0.1"))
	float ScatterPhaseAggroClamp = 48.f;

	/** 산포 종료 후 적용되는 어그로 반(cm). EnemyBase 규칙에 따라 0이면 무한 추격 범위 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|SwarmScatter")
	float ChaseAggroRadiusAfterScatter = 0.f;

	/** MoveTo 수용 거리(cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|SwarmScatter", meta = (ClampMin = "16.0"))
	float ScatterMoveAcceptanceRadius = 72.f;

	/** 산포 중 TrackLight FSM 비활성 전투(빛 즉사)와 겹치는 연출 줄이기 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|SwarmScatter")
	bool bDisableLightTrackingWhileScattering = true;

private:
	UFUNCTION()
	void HandleWaveSpawnQueueEmptied(int32 WaveIndex, AEnemySpawner* Spawner);

	void BindSwarmDelegates();
	void IssueRandomScatterDestination();
	bool TryPickRandomScatterPoint(FVector const& Origin, FVector& OutLocation) const;
	void TransitionToChasePhase();
	void CleanupSwarmDelegates();

	UPROPERTY()
	TWeakObjectPtr<AEnemySpawner> OwningSwarmSpawner;

	bool bSwarmDelegateBound = false;
	bool bScatterChaseHoldActive = false;
	float ScatterPhaseEndsAtWorldSeconds = -1.f;
	float ScatterMoveCooldownRemaining = 0.f;
	float CachedChaseAggroRadius = 0.f;
	bool bCachedLightTracking = true;
	bool bHasCachedChaseAggroRadius = false;

	/** swarm 전용 공격 쿨다운 트래킹 — 베이스의 LastAttackTime은 private라 직접 못 씀. */
	float LastSwarmAttackTime = -BIG_NUMBER;
};
