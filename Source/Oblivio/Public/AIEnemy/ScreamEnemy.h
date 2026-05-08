#pragma once

// =============================================================================
// AScreamEnemy — "벽 속의 비명"
//
// 핵심 능력 사이클(고정 순서):
//   1) Charging  : ChargeSeconds(기본 5초) 동안 제자리 텔레그래프. 시작 순간
//                  플레이어 월드 좌표를 SnapshotLocation 으로 고정.
//   2) Transit   : TransitSeconds(기본 0.18초) 동안 현재 위치 → SnapshotLocation
//                  으로 직선 초고속 보간(벽 통과). 한 프레임 텔레포트가 아니라
//                  연속 위치 업데이트로 “순식간에 끊겨 이동” 체감.
//   3) Attack    : 도착 직후 PerformAttack(TargetActor) 1회 → 전투 시스템 위임.
//   4) Root      : PostBlinkRootSeconds(기본 3초) 동안 착지 좌표에 고정 — 이동/추격
//                  입력 없음. 플레이어의 반격·거리 벌리기 윈도.
//   5) Cooldown  : AbilityCooldownSeconds(기본 90초) 동안 능력 봉인. 이 동안은
//                  EnemyBase 와 동일하게 주변 배회·추격을 수행하되, 콜리전을
//                  WorldStatic Ignore + Flying 으로 바꿔 "벽 무시" 체감을 유지한다.
//
// 능력 페이즈는 EEnemyAIState FSM 위에 얹혀 있다. Charging/Transit/Root 동안에는
// 베이스의 UpdateChase/Attack/Idle 호출을 무시하고 우리 Tick 이 직접 운행한다.
// Ready/Cooldown 동안에는 베이스 FSM(추격/배회 등)을 그대로 사용한다.
// =============================================================================

#include "CoreMinimal.h"
#include "AIEnemy/EnemyBase.h"
#include "ScreamEnemy.generated.h"

UENUM(BlueprintType)
enum class EScreamAbilityPhase : uint8
{
	Ready       UMETA(DisplayName = "Ready"),
	Charging    UMETA(DisplayName = "Charging"),
	Transit     UMETA(DisplayName = "Transit"),
	ChainPause  UMETA(DisplayName = "ChainPause"),  // P2: 1차 블링크 후 짧은 정지 → 2차 블링크
	Root        UMETA(DisplayName = "Root"),
	Cooldown    UMETA(DisplayName = "Cooldown"),
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FScreamAbilityPhaseChangedSignature, class AScreamEnemy*, Enemy, EScreamAbilityPhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FScreamSnapshotTakenSignature, class AScreamEnemy*, Enemy, FVector, SnapshotLocation);

UCLASS(Blueprintable)
class OBLIVIO_API AScreamEnemy : public AEnemyBase
{
	GENERATED_BODY()

public:
	AScreamEnemy();

	virtual void Tick(float DeltaSeconds) override;
	virtual void Die() override;

	/** Ghost 이동(벽 통과)을 항상 사용하므로 NavMesh 기반 막힘 복구를 비활성화한다. */
	virtual bool IsStuckRecoveryEnabled()   const override { return false; }
	virtual bool IsObstacleAttackEnabled()  const override { return false; }

	UFUNCTION(BlueprintPure, Category = "Enemy|Scream")
	EScreamAbilityPhase GetAbilityPhase() const { return AbilityPhase; }

	UFUNCTION(BlueprintPure, Category = "Enemy|Scream")
	float GetAbilityCooldownRemaining() const { return CooldownRemaining; }

	UFUNCTION(BlueprintPure, Category = "Enemy|Scream")
	FVector GetSnapshotLocation() const { return SnapshotLocation; }

	/** true 면 페이즈 2(체력 50% 이하) — 사이클당 블링크가 2회로 증가. */
	UFUNCTION(BlueprintPure, Category = "Enemy|Scream")
	bool IsPhaseTwo() const { return bIsPhaseTwo; }

	/** 라이트 누적 경직이 활성화된 동안 true. */
	UFUNCTION(BlueprintPure, Category = "Enemy|Scream|CC")
	bool IsLightStunActive() const;

	/** 라이트 면역 윈도가 활성화된 동안 true(경직 진행 중에도 true). */
	UFUNCTION(BlueprintPure, Category = "Enemy|Scream|CC")
	bool IsLightImmune() const;

	/** 0~LightStunBuildupSeconds. UI/디버깅용. */
	virtual float GetLightExposureAccum() const override { return LightExposureAccum; }

	/** 일반 전투 경직 무시(Whisper/Luxeater 패턴). 라이트 누적 경직은 내부에서 별도로 처리. */
	virtual void ApplyCCStun(float Duration = 0.0f) override;

	/** 광원 누적값 갱신 — 임계 도달 시 라이트 스턴 트리거. */
	virtual void OnLightHit(float Intensity, float Duration) override;

	UPROPERTY(BlueprintAssignable, Category = "Enemy|Scream|Events")
	FScreamAbilityPhaseChangedSignature OnAbilityPhaseChanged;

	UPROPERTY(BlueprintAssignable, Category = "Enemy|Scream|Events")
	FScreamSnapshotTakenSignature OnSnapshotTaken;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void UpdateChase() override;
	virtual void UpdateAttack() override;
	virtual void UpdateIdle(float DeltaSeconds) override;

	/** 체력 변화 후 페이즈 갱신(Luxeater 패턴 차용). */
	virtual void NotifyEnemyDamageApplied(float AppliedDamage) override;

	/** 차지 시간(초). 시작 순간 플레이어 좌표를 스냅샷한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Scream|Ability", meta = (ClampMin = "0.1"))
	float ChargeSeconds = 5.0f;

	/** Transit(블링크) 보간 시간(초). 0.08~0.25 권장. 0이면 한 프레임 텔레포트가 되므로 기획상 비권장. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Scream|Ability", meta = (ClampMin = "0.0"))
	float TransitSeconds = 0.18f;

	/** Transit 직후 착지 좌표에 고정되는 시간(초). 플레이어 반격 윈도. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Scream|Ability", meta = (ClampMin = "0.0"))
	float PostBlinkRootSeconds = 3.0f;

	/** 능력 사이클 종료 후 다시 차지 가능까지 대기 시간(초). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Scream|Ability", meta = (ClampMin = "0.0"))
	float AbilityCooldownSeconds = 30.0f;

	/**
	 * true 면 게임 시작 시 즉시 차지가 아니라 한 번의 쿨다운(AbilityCooldownSeconds)부터 시작한다.
	 * 의도: 플레이어가 방에 들어오자마자 블링크 당하지 않고, 처음 한 사이클은 wall-ignore wander 로 압박한 뒤 첫 블링크가 발동.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Scream|Ability")
	bool bStartOnCooldown = true;

	/** 차지 시작을 허용하는 최소/최대 거리(cm). 너무 가까우면 의미 없음, 너무 멀면 페어성 저하. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Scream|Ability", meta = (ClampMin = "0.0"))
	float MinChargeStartDistance = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Scream|Ability", meta = (ClampMin = "100.0"))
	float MaxChargeStartDistance = 2400.0f;

	/** Cooldown 진입 후 다음 차지 평가까지 추가 휴지(초). 0이면 즉시 평가. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Scream|Ability", meta = (ClampMin = "0.0"))
	float ReadyArmDelay = 0.5f;

	/** Charging 동안 타겟을 향해 회전할 보간 속도(도/초). 0이면 즉시 정렬. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Scream|Ability", meta = (ClampMin = "0.0"))
	float ChargeFaceTargetRotateRate = 540.0f;

	/** Cooldown 동안 캡슐의 WorldStatic 충돌을 Ignore + Flying 모드로 전환해 "벽 무시" 체감. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Scream|Ghost")
	bool bGhostMovementDuringCooldown = true;

	/** Ghost 모드에서의 비행 이속(cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Scream|Ghost", meta = (ClampMin = "0.0"))
	float GhostFlySpeed = 240.0f;

	/** Ghost 배회 목표 재선정 간격(초). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Scream|Ghost", meta = (ClampMin = "0.5"))
	float GhostWanderRetargetInterval = 3.5f;

	/** Ghost 배회 시 스폰/타겟 주변 샘플 반경(cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Scream|Ghost", meta = (ClampMin = "100.0"))
	float GhostWanderRadius = 800.0f;

	/** Ghost 배회 시 Z 방향 이탈 한계(cm). 스폰 Z 기준 +-이 값으로 제한해 천장/지하로 빠지지 않게 한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Scream|Ghost", meta = (ClampMin = "0.0"))
	float GhostVerticalClamp = 200.0f;

	// === Phase 2 (HP <= 50%) — 더블 블링크 ===

	/** 페이즈 2 진입 체력 비율(0~1). 기본 0.5 = 절반 이하. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Scream|Phase2", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float PhaseTwoHealthPercent = 0.5f;

	/** 페이즈 2 사이클당 블링크 횟수. 기본 2(차지 → 1차 Transit → ChainPause → 2차 Transit → Attack). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Scream|Phase2", meta = (ClampMin = "1", ClampMax = "5"))
	int32 PhaseTwoBlinksPerCycle = 2;

	/** 1차 Transit 직후 짧은 정지 시간(초). 0이면 곧바로 2차 Transit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Scream|Phase2", meta = (ClampMin = "0.0"))
	float ChainPauseSeconds = 0.25f;

	// === CC: 광원 노출 누적 → 경직 → 면역 ===

	// LightStunBuildupSeconds, LightStunDuration 은 EnemyBase에서 상속.
	// 기본값을 ScreamEnemy에 맞게 생성자에서 재설정한다.

	/** 스턴 종료 후 라이트 스턴 면역 시간(초). 기본 30초. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Scream|CC", meta = (ClampMin = "0.0"))
	float LightStunImmunitySeconds = 30.0f;

	/** 새 OnLightHit 이 이 시간(초) 동안 들어오지 않으면 누적값을 빠르게 감소시킨다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Scream|CC", meta = (ClampMin = "0.0"))
	float LightExposureGraceSeconds = 0.4f;

	/** Grace 만료 후 누적 감소 속도 배수(초당 LightStunBuildupSeconds 의 몇 배만큼 깎을지). 기본 2.0 → 누적 3초가 1.5초만에 0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Scream|CC", meta = (ClampMin = "0.0"))
	float LightExposureDecayRate = 2.0f;

	/** 디버그: 현재 능력 페이즈/스냅샷 좌표/Transit 보간 라인을 그린다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Scream|Debug")
	bool bDebugDrawAbility = false;

private:
	void TransitionToPhase(EScreamAbilityPhase NewPhase);

	bool TryStartChargeIfReady();
	void StartCharge();
	void TickCharge(float DeltaSeconds);
	void StartTransit();
	void TickTransit(float DeltaSeconds);
	void FinishTransitAndAttack();
	void StartChainPause();
	void TickChainPause(float DeltaSeconds);
	void StartRoot();
	void TickRoot(float DeltaSeconds);
	void StartCooldown();
	void TickCooldown(float DeltaSeconds);
	void EndCooldown();

	// CC
	void UpdateLightStunState(float DeltaSeconds);
	void TriggerLightStun();
	void UpdateHealthPhase();

	void EnableGhostMovement(bool bEnable);
	void TickGhostWander(float DeltaSeconds);
	bool PickGhostWanderTarget(FVector& OutTarget) const;

	void FaceTarget(float DeltaSeconds, float RotateRateDegPerSec);

	bool IsAbilityActive() const
	{
		return AbilityPhase == EScreamAbilityPhase::Charging ||
			AbilityPhase == EScreamAbilityPhase::Transit ||
			AbilityPhase == EScreamAbilityPhase::ChainPause ||
			AbilityPhase == EScreamAbilityPhase::Root;
	}

protected:
	/** 현재 능력 페이즈. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Scream|State")
	EScreamAbilityPhase AbilityPhase = EScreamAbilityPhase::Ready;

	/** 페이즈 2(체력 50% 이하) 진입 여부. 한 번 true 가 되면 회복돼도 true 유지. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Scream|State")
	bool bIsPhaseTwo = false;

private:
	/** 차지 시작 순간 기록한 플레이어 월드 좌표. */
	FVector SnapshotLocation = FVector::ZeroVector;

	/** Transit 시작 시 자기 위치(직선 보간의 시작점). */
	FVector TransitStart = FVector::ZeroVector;

	/** 현재 페이즈 진입 후 경과 시간(초). */
	float PhaseElapsed = 0.0f;

	/** Cooldown 잔여(초). 0 이하 → Ready 전이. */
	float CooldownRemaining = 0.0f;

	/** Cooldown 진입 직후 ReadyArmDelay 동안 차지 평가를 미루는 타이머. */
	float ReadyArmDelayRemaining = 0.0f;

	/** Ghost 배회 재선정 카운트다운. */
	float GhostWanderRetargetCooldown = 0.0f;
	FVector CurrentGhostWanderGoal = FVector::ZeroVector;
	bool bHasGhostWanderGoal = false;

	/** Spawn Z 기준값(Ghost Z 클램프용). */
	float SpawnZ = 0.0f;

	/** 현재 사이클에서 남은 블링크 횟수. StartCharge 시 페이즈에 따라 1 또는 PhaseTwoBlinksPerCycle 로 세팅. */
	int32 BlinksRemainingInCycle = 0;

	// === Light CC 내부 상태 ===

	/** 광원 누적 노출(초). LightStunBuildupSeconds 도달 시 라이트 스턴 트리거 후 0으로 리셋. */
	float LightExposureAccum = 0.0f;

	/** 가장 최근 OnLightHit 의 월드 시각(초). Grace 만료 판단용. */
	double LastLightHitWorldTime = 0.0;

	/** 라이트 스턴이 풀리는 월드 시각(초). 현재 시각 < 이 값 이면 스턴 진행 중. */
	double LightStunUntilSec = 0.0;

	/** 라이트 면역 윈도가 풀리는 월드 시각(초). 현재 시각 < 이 값 이면 면역. */
	double LightImmuneUntilSec = 0.0;

	/** Ghost 모드 진입 전 원본 콜리전 응답/이동 모드 캐시. */
	bool bGhostActive = false;
	TEnumAsByte<enum ECollisionResponse> CachedWorldStaticResponse = ECR_Block;
	TEnumAsByte<enum ECollisionResponse> CachedMeshWorldStaticResponse = ECR_Block;
	uint8 CachedMovementMode = 0;
	float CachedMaxFlySpeed = 600.0f;
	bool bCachedSweepDuringMove = true;
};
