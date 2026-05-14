#pragma once

// =============================================================================
// ATankEnemy — 느리고 튼튼한 변형. 근타격은 UEnemyMeleeCommitNotify 등 기존 경로 필요.
// 심작 박동: ALuxeaterEnemy::TickBossAbilities 처럼 Super::Tick 전·후로 시도해 Chase/장애물 분기와 동기화.
// =============================================================================

#include "CoreMinimal.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "AIEnemy/EnemyBase.h"
#include "TimerManager.h"
#include "TankEnemy.generated.h"

class UMaterialInterface;
class UAnimMontage;
class UNiagaraSystem;


/** 탱커 기본형 — Basic 대비 체력↑·이동↓·공격력 소폭↑. 세부값은 BP에서 조정. */
UCLASS(Blueprintable)
class OBLIVIO_API ATankEnemy : public AEnemyBase
{
	GENERATED_BODY()

public:
	ATankEnemy();

	/** 심장 스태틱 메시. 기본은 왼손 소켓에 자동 부착(AttachTankHeartMeshToLeftHand). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Heart")
	TObjectPtr<UStaticMeshComponent> TankHeartMeshComponent;

	/** 데미지 노티 시 붉게 점등(심장 메시 자식). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Heart")
	TObjectPtr<UPointLightComponent> TankHeartPulseLight;

	/** 비어 있으면 hand_l 등 왼손 후보 소켓을 순서대로 시도합니다. 스켈에 맞는 소켓이 있으면 BP에서 지정하면 됩니다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Heart")
	FName TankHeartAttachSocketName;

	/** 심장 메시 표시/숨김. true일 때 소켓에 다시 Attach 해 손 위치를 맞춤 */
	UFUNCTION(BlueprintCallable, Category = "Tank|Heart")
	void SetTankHeartMeshVisible(bool bVisible);

	/**
	 * 심장박동 데미지 애님 노티에서 호출 — 붉은 빛이 피크로 박동 후 감쇠.
	 * 전용 서버에서는 스킵; 시뮬/클라/리슨 서버에서만 연출.
	 */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Tank|Heartbeat")
	void PulseTankHeartDamageFlash();

	virtual void Die() override;

	/** 기본 AggroRadius 진입 시부터는 거리와 무관하게 추격(플레이어가 죽거나 타겟이 없어질 때까지). 심작 AoE 보조 판정 유지. */
	virtual bool HasValidAggroTarget() const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** ABP 상태 머신 / UTankEnemyAnimInstance: 심작 박동 연출 재생 분기용. heartbeat 비활성이면 항상 false */
	UFUNCTION(BlueprintPure, Category = "Enemy|Tank|Heartbeat")
	bool IsTankHeartbeatChannelingForAnim() const;

	UFUNCTION(BlueprintPure, Category = "Enemy|Tank|Heartbeat")
	bool UsesHeartbeatAoEAttack() const;

	virtual EEnemyAIState GetEnemyState() const override;

	/**
	 * 심장 박동 Anim Notify — 한 번의 박동 피해. UTankHeartbeatDamageType 으로 ApplyDamage.
	 * 횟수는 애님에 노티 개수대로(예: 3개 배치).
	 */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Tank|Heartbeat")
	void ApplyHeartbeatDamageFromAnimNotify();

	/**
	 * 심작 몽타주/시퀀스 끝에 배치 — 채널링 종료·쿨 적용. 서버 권한에서만 FSM 처리.
	 * (노티를 놓치면 HeartbeatChannelFailSafeSeconds 타이머로 정리)
	 */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Tank|Heartbeat")
	void FinishHeartbeatAttackFromAnimNotify();

	/** 애님 노티 타이밍(30fps 프레임 15/33/60) 및 심작 종료 후 몽타주와 연결됩니다. */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Tank|JumpAttack")
	void JumpAttack_NotifyLiftOff();

	UFUNCTION(BlueprintCallable, Category = "Enemy|Tank|JumpAttack")
	void JumpAttack_NotifyLandingImpact();

	UFUNCTION(BlueprintCallable, Category = "Enemy|Tank|JumpAttack")
	void JumpAttack_NotifyMontageFinished();

	/** ABP 상태 머신: 점프 공격 패턴 활성 여부(GetEnemyState==JumpAttack과 동등). */
	UFUNCTION(BlueprintPure, Category = "Enemy|Tank|JumpAttack")
	bool IsTankJumpAttackFsmActiveForAnim() const;

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayTankJumpMontage();

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_SpawnJumpRingBurst(FVector BurstLocationFloor);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	/** 왼손 소켓(후보 순회 또는 TankHeartAttachSocketName)에 심장 메시 부착 */
	void AttachTankHeartMeshToLeftHand();

	/** 베이스와 동일(MoveTo). 심작은 Tick(MaybeTryTankHeartbeatAoE)에서 먼저 시도 */
	virtual void UpdateChase() override;
	virtual void DrawDebugCombatExtras() override;

	virtual bool ShouldSuppressAILocomotion() const override;

	/** 어그로 유지 시 FSM — 심작 채널링 중이면 Heartbeat(ABP·블루프린트의 Enemy State 핀과 일치). */
	virtual EEnemyAIState SelectStateWhileAggroed() const override;
	virtual bool TryConsumeSpecialFSMUpdate() override;

	/** 탱커는 심작 AoE 표시를 사용하므로 베이스 근접 범위 메시는 쓰지 않음. */
	virtual bool ShouldShowMeleeAttackRangeIndicator() const override { return false; }

	/**
	 * 심작 채널링(bHeartbeatChanneling, 복제됨) 동안만 표시.
	 * 바닥 원 디스크 반경은 HeartbeatAoERadius(cm); 피해는 동일 반경의 3D 구.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Heartbeat|Visual")
	bool bShowHeartbeatAoERangeIndicator = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tank|Heartbeat|Visual")
	TObjectPtr<UMaterialInterface> HeartbeatAoERangeIndicatorMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Heartbeat|Visual", meta = (ClampMin = "0.01"))
	float HeartbeatAoERangeIndicatorBuiltInSphereRadiusUU = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Heartbeat|Visual", meta = (ClampMin = "0.001", ClampMax = "2.0"))
	float HeartbeatAoERangeIndicatorDiskThicknessScale = 0.02f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Heartbeat|Visual")
	TObjectPtr<UStaticMeshComponent> HeartbeatAoERangeIndicatorMesh;

	/**
	 * 점프 착지 슬램 AoE 디스크(월드 XY는 TankJumpLandingFloorWorld 로 동기화, 반경 스케일은 TankJumpSlamRadiusCm).
	 * 바깥 링 파동은 TankJumpRingOuterCm 과 Niagara 등으로 표현됩니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|JumpAttack|Visual")
	bool bShowJumpLandingAoETelegraph = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tank|JumpAttack|Visual")
	TObjectPtr<UMaterialInterface> JumpLandingAoERangeIndicatorMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|JumpAttack|Visual", meta = (ClampMin = "0.01"))
	float JumpLandingAoERangeIndicatorBuiltInSphereRadiusUU = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|JumpAttack|Visual", meta = (ClampMin = "0.001", ClampMax = "2.0"))
	float JumpLandingAoERangeIndicatorDiskThicknessScale = 0.02f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|JumpAttack|Visual")
	TObjectPtr<UStaticMeshComponent> JumpLandingAoERangeIndicatorMesh;

	/** PIE/게임: 근접 AttackRange·심작 Heartbeat AoE 구 시각화(Aggro 디버그와 별개). Shipping 빌드 제외. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Tank|Debug", meta = (DisplayName = "Debug Draw Combat Ranges"))
	bool bDebugDrawCombatRanges = true;

private:
	/**
	 * true면 추격(Chase) 중에 Heartbeat AoE(범위·쿨)를 시도. 근접 Attack은 베이스 FSM 그대로(PerformAttack / 근접 노티).
	 * BP에 false가 저장된 경우 BeginPlay에서 true로 맞춤. 근접·심작 모두 끄려면 BeginPlay 이후 false.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|Heartbeat",
		meta = (AllowPrivateAccess = "true", DisplayName = "Use Heartbeat AoE Attack"))
	bool bUseHeartbeatAoEAttack = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|Heartbeat",
		meta = (AllowPrivateAccess = "true", DisplayName = "Heartbeat Damage Per Notify"))
	float HeartbeatPulseDamage = 3.0f;

	/** 피해 패턴 종료 후 다음 심작까지(초). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|Heartbeat", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float HeartbeatCooldownSeconds = 10.f;

	/**
	 * 심작 애님이 끝날 때 FinishHeartbeatAttackFromAnimNotify 노티를 넣지 않으면 이 시간(초) 후 강제 종료.
	 * 0 이면 비활성(애님 종료 노티 필수).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|Heartbeat",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0", DisplayName = "Heartbeat Fail-Safe Duration (sec)"))
	float HeartbeatChannelFailSafeSeconds = 12.f;

	/** 심작이 타겟에게 닿는 반경(cm). Chase 중 이 안이면 심작 시도 가능. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Tank|Heartbeat", meta = (AllowPrivateAccess = "true", ClampMin = "1.0",
		DisplayName = "Heartbeat AoE Radius (cm)"))
	float HeartbeatAoERadius = 1400.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|Heartbeat", meta = (AllowPrivateAccess = "true"))
	bool bHeartbeatUseHorizontalDistance = true;

	FTimerHandle HeartbeatFailSafeTimerHandle;

	/** 서버에서만 피해/타이머, 클라는 ABP 분기용 복제. 심장 표시는 TankHeartShow/Hide 노티 담당. */
	UPROPERTY(ReplicatedUsing = OnRep_HeartbeatChanneling)
	bool bHeartbeatChanneling = false;

	/** 한 번 AggroRadius 안에 들어오면 Death·타겟 소실 전까지 추격 유지(서버가 잠금, 복제). */
	UPROPERTY(Replicated)
	bool bTankStickyAggroUntilDeath = false;

	/** 복제로 채널링 끝날 때 심장 숨김만(표시는 애님 Show 노티). */
	UFUNCTION()
	void OnRep_HeartbeatChanneling();
	float LastHeartbeatSequenceEndWorldTime = -BIG_NUMBER;

	void TryStartHeartbeatSequence();
	void FinishHeartbeatSequence();
	void ClearHeartbeatTimers();

	bool IsTargetInHeartbeatAoERange() const;
	/** 심작 피해 노티: HeartbeatAoERadius·bHeartbeatUseHorizontalDistance 를 FSM/범위 표시와 동일하게 사용 */
	bool IsTargetInHeartbeatDamageRange() const;
	bool IsHeartbeatCooldownReady(float NowWorldSeconds) const;
	bool TryStartHeartbeatWhenReady();

	/** 어그로·쿨·범위 OK 시 심작 개시. Tick 전후·여러 경로에서 호출, 가드 동일 */
	void MaybeTryTankHeartbeatAoE();

	void UpdateHeartbeatAoERangeIndicatorVisual();

	void TickHeartPulseLightFade();
	void ClearHeartPulseFlashTimer();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|Heartbeat", meta = (AllowPrivateAccess = "true",
		DisplayName = "Heart Pulse Light Peak (cd)", ClampMin = "0.0"))
	float HeartPulseLightPeakCandela = 18000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|Heartbeat", meta = (AllowPrivateAccess = "true",
		DisplayName = "Heart Pulse Light Radius (cm)", ClampMin = "1.0"))
	float HeartPulseLightAttenuationRadius = 280.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|Heartbeat", meta = (AllowPrivateAccess = "true", ClampMin = "0.01"))
	float HeartPulseFadeTickSeconds = 0.03f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|Heartbeat", meta = (AllowPrivateAccess = "true",
		ClampMin = "0.0", ClampMax = "1.0"))
	float HeartPulseFadeMultiplierPerTick = 0.78f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|Heartbeat", meta = (AllowPrivateAccess = "true",
		ClampMin = "0.0"))
	float HeartPulseFadeStopBelowCandela = 120.f;

	FTimerHandle HeartPulseFadeTimerHandle;
	float HeartPulseLightFadeCurrent = 0.f;

	bool bLoggedTankHeartStaticMeshMissing = false;

	// -------------------------------------------------------------------------
	// 점프 착지 공격(심작 직후, 애님 30fps 2.00s 프레임 15·20·33·60 과 동기 노티 필요)
	// -------------------------------------------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|JumpAttack|Setup",
		meta = (AllowPrivateAccess = "true", DisplayName = "Jump Attack After Heartbeat"))
	bool bUseTankJumpAttackAfterHeartbeat = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|JumpAttack|Setup",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimMontage> TankJumpAttackMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|JumpAttack|Setup",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraSystem> TankJumpRingNiagaraSystem;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|JumpAttack|Damage",
		meta = (AllowPrivateAccess = "true", ClampMin = "1.0", DisplayName = "Slam Damage"))
	float TankJumpSlamDamage = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|JumpAttack|Damage",
		meta = (AllowPrivateAccess = "true", ClampMin = "1.0", DisplayName = "Slam Radius (cm)"))
	float TankJumpSlamRadiusCm = 220.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|JumpAttack|Damage",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0", DisplayName = "Ring Wave Damage"))
	float TankJumpRingWaveDamage = 16.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|JumpAttack|Damage",
		meta = (AllowPrivateAccess = "true", ClampMin = "1.0", DisplayName = "Ring Outer Radius (cm)"))
	float TankJumpRingOuterCm = 900.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|JumpAttack|Damage",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0",
			DisplayName = "Slam: Max Victim Feet Above Floor (cm)"))
	float TankJumpSlamVictimMaxFeetCmAboveLanding = 95.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|JumpAttack|Damage",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0", DisplayName = "Ring: Victim Vertical Clear (cm)"))
	float TankJumpRingVictimJumpClearCmAboveLanding = 115.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|JumpAttack|Arc",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0", DisplayName = "Arc Peak Z Delta (cm)"))
	float TankJumpPeakZDeltaCm = 450.f;

	/** 리프트오프 순간 서버에서 캡슐을 위로 순간 이동(월드 공중 궤적 시작점). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|JumpAttack|Arc",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0", DisplayName = "Lift-off Instant Z (cm)"))
	float TankJumpLiftOffInstantZCm = 120.f;

	/** 플레이어 발밑 XY 에서 분리 후 착지 — 탱·플레이어 캡슐 XY 겹침 방지 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|JumpAttack|Landing",
		meta = (AllowPrivateAccess = "true", DisplayName = "Avoid Overlap With Player Capsule"))
	bool bTankJumpLandingAvoidPlayerCapsuleOverlap = true;

	/** 착지까지 최소 거리(cm) = 플레이어 캡슐반경 + 탱커 캡슐반경 + 이 값 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|JumpAttack|Landing",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0",
			DisplayName = "Landing Extra Radial Gap (cm)"))
	float TankJumpLandingExtraRadialGapCm = 72.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|JumpAttack|Timers",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float TankJumpFailsafeSeconds = 2.2f;

	/** 이륙 노티 놓치면 이 시간(sec)부터 곡선 호흡을 강제(30fps 프레임 15). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|JumpAttack|Timers",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float TankJumpForcedLiftOffAfterMontageStartsSeconds = 0.55f;

	/** 몽타주 시작 후 착지 강제 대체(프레임 33 → 33/30 초). 노티 놓치면 보정. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|JumpAttack|Timers",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float TankJumpForcedLandingAfterMontageStartsSeconds = 1.13333333f;

	UPROPERTY(ReplicatedUsing = OnRep_TankJumpAttackActive)
	bool bTankJumpAttackActive = false;

	UPROPERTY(Replicated)
	bool bTankJumpShowLandingTelegraph = false;

	UPROPERTY(Replicated)
	FVector TankJumpLandingFloorWorld = FVector::ZeroVector;

	/** bTankJumpAttackActive 복제 시 클라 Idle/Chase 로코 앰비언트가 Jump 까지 따라붙도록 동기화. */
	UFUNCTION()
	void OnRep_TankJumpAttackActive();

	FTimerHandle TankJumpFailsafeTimerHandle;
	FTimerHandle TankJumpLiftOffFailsafeTimerHandle;
	FTimerHandle TankJumpLandingFailsafeTimerHandle;

	bool bTankJumpKinematicAscent_Server = false;
	bool bTankJumpLandingDamageCommitted_Server = false;
	double TankJumpLiftOffStampServerSecs = -1.e20;

	FVector TankJumpArcBeginWorld = FVector::ZeroVector;

	/** 심작 종료 직후 점프 패턴. 심작 AoE 밖이어도 TargetActor 유효 시 시전. */
	bool TryBeginTankJumpAttackAfterHeartbeat();
	bool StartTankJumpAttackSequence_Server();
	void CompleteTankJumpAttackSequence_Server();
	void ClearTankJumpTimers();
	void OnTankJumpFailsafe_Server();
	void TankJumpLiftOffFailsafe_Server();
	void TankJumpLandingFailsafe_Server();

	void TickTankJumpArc_Server();
	void LiftOffJumpAttack_Server_Impl();
	void ApplyJumpSlamAndRing_Server(const FVector& LandFloorWorld);

	void UpdateJumpLandingAoEIndicatorVisual();

	static bool TankJumpTraceLandscapeFloor(UWorld* World, AActor* IgnoreActor, AActor* TraceAlsoIgnoreActor,
		FVector WorldProbePt, FVector& OutFloorImpact);
	static float TankJumpResolveFeetCmAboveLandingZ(AActor const* Victim, float LandingFloorWorldZ);

	FVector ComputeJumpLandingFeetProbeWorld_Server() const;

	void SnapshotJumpLandingFloorFromActor_Server(FVector ProbeLocation, AActor* TraceAlsoIgnoreActor);
};
