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
class UStaticMesh;
class UAnimSequence;
class ATankMembraneEmitterActor;
class ATankMembraneProjectile;
class ATankPlacentaShellActor;


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

	/** 기본 AggroRadius 진입 시부터 sticky 추격 거리 무시. 조우 판에는 시야(LOS) 포함. 심작 AoE 거리만으로 추격 시작하지 않음. */
	virtual bool HasValidAggroTarget() const override;

	/** 거리 원통(IsAggroDistanceToTargetInsideCylinderIgnoringLos)+옵션 LOS 통과 시에만 true. 스티키 잠금은 HasValid 에서 분리. */
	virtual bool IsAggroDistanceSatisfiedForTarget() const override;

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

	/** LiftOff / Landing / Finish 애님 노티 타이밍. 연출 재생은 ABP(애님 시퀀스 상태)에서 처리. */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Tank|JumpAttack")
	void JumpAttack_NotifyLiftOff();

	UFUNCTION(BlueprintCallable, Category = "Enemy|Tank|JumpAttack")
	void JumpAttack_NotifyLandingImpact();

	/** 애님 시퀀스가 끝났을 때(Finish 노티 또는 서버 타임아웃 동일 처리). */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Tank|JumpAttack")
	void JumpAttack_NotifyMontageFinished();

	/** 양막 애님 시퀀스의 소환 타이밍 노티. 이 시점에만 양막 이펙터/투사체를 실제 생성합니다. */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Tank|Membrane")
	void TankMembrane_NotifySummon();

	/** 양막 연출 피니시 노티(Tank Membrane Finished) — 패턴 종료는 이 노티(+이펙터 웨이브 종료)를 따릅니다. */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Tank|Membrane")
	void TankMembrane_NotifyMontageFinished();

	/** 태반 패턴 활성 분기(ATankEnemy / UTankEnemyAnimInstance). */
	UFUNCTION(BlueprintPure, Category = "Enemy|Tank|Placenta")
	bool IsTankPlacentaDefenseActiveForAnim() const;

	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator,
		AActor* DamageCauser) override;

	virtual void DispatchEnemyAttackCommitted(AActor* Target, float DamageAmount,
		TSubclassOf<UDamageType> DamageTypeClass) override;

	void ApplyHealth(float Damage) override;

	/** 로코용: 양막 패턴 복제 구간(bTankMembranePatternCycleActive) 활성. */
	UFUNCTION(BlueprintPure, Category = "Enemy|Tank|Membrane")
	bool IsTankMembraneFsmActiveForAnim() const;

	/** ABP 상태 머신: 점프 공격 패턴 활성 여부(GetEnemyState==JumpAttack과 동등). */
	UFUNCTION(BlueprintPure, Category = "Enemy|Tank|JumpAttack")
	bool IsTankJumpAttackFsmActiveForAnim() const;

	/** ATankMembraneEmitterActor 가 부채꼴 3발 완료 후 호출(서버). */
	void NotifyTankMembraneEmitterFinished(ATankMembraneEmitterActor* Emitter);

	/** 서버만 — 태반 액터 파괴 시 조기 종료. */
	void NotifyTankPlacentaShellBroken_Server(ATankPlacentaShellActor* BrokenShell);

	/** 로코 앰비언트 등 동기만 — 점프 애니는 복제된 bTankJumpAttackActive 로 ABP가 재생. */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_SyncTankJumpAttackStart();

	/**
	 * 점프 착지 슬램 AoE 디스크만 착지 **전** 표시(Walk 닿기 전, bTankJumpShowLandingTelegraph).
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
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Tank|UI")
	FText BossDisplayName = FText::FromString(TEXT("The Great Bloated Fetus"));
	
	UFUNCTION(BlueprintImplementableEvent, Category = "Enemy|Tank|UI")
	void OnShowBossHP(const FText& InBossName, float InCurrentHP, float InMaxHP);
	
	UFUNCTION(BlueprintImplementableEvent, Category = "Enemy|Tank|UI")
	void OnUpdateBossHP(float InCurrentHP, float InMaxHP);
	
	UFUNCTION(BlueprintImplementableEvent, Category = "Enemy|Tank|UI")
	void OnHideBossHP();

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

	/** 손전등 추적: 스포트→몸 표본까지 벽에 가려지면 추적하지 않음. */
	virtual bool PassesEnemyAdditionalFlashlightTrackLineOfSight(USpotLightComponent const* Spot,
		FVector const& EnemyLightSampleWorld) const override;

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

	/** 탱커 피해야 할 심장 박동/원형 AoE 피해·시도 거리(cm). 추격 거리 AggroRadius 와 무관해야 함 — 조우 거리에는 사용하지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Tank|Heartbeat", meta = (AllowPrivateAccess = "true", ClampMin = "1.0",
		DisplayName = "Heartbeat AoE Radius (cm)"))
	float HeartbeatAoERadius = 1400.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|Heartbeat", meta = (AllowPrivateAccess = "true"))
	bool bHeartbeatUseHorizontalDistance = true;

	FTimerHandle HeartbeatFailSafeTimerHandle;

	/** 서버에서만 피해/타이머, 클라는 ABP 분기용 복제. 심장 표시는 TankHeartShow/Hide 노티 담당. */
	UPROPERTY(ReplicatedUsing = OnRep_HeartbeatChanneling)
	bool bHeartbeatChanneling = false;

	/** AggroRadius 진입 또는 플레이어가 반경 밖에서 유효 피해 1회면 Death·타겟 소실 전까지 추격 유지(서버 잠금, 복제). */
	UPROPERTY(Replicated)
	bool bTankStickyAggroUntilDeath = false;

	/** 거리 원통 충족 후에 어그로/조우 가능하려면 눈-플레이어 사이 라인 트레이스에 지형이 있는지 차단해야 함(true). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|Perception",
		meta = (AllowPrivateAccess = "true"))
	bool bTankRequireLineOfSightForAggroCylinder = true;

	/** TrackLight(FSM 손전등 추적)·빛 샘플 판정 시 스포트→몸 표본 사이 Visibility 로 벽 차단 여부 검사 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|Perception",
		meta = (AllowPrivateAccess = "true"))
	bool bTankRequireLineOfSightForFlashlightTracking = true;

	/** Visibility 채널 라인 트레이스: 차단판정 시 목표 캡슐보다 최소 여유 거리 필요(부동 소수)·cm≈UU */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|Perception",
		meta = (AllowPrivateAccess = "true", ClampMin = "1.0"))
	float TankThreatLosClearanceUU = 45.f;

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

	bool IsTankLosBlockedTowardsActor(const AActor* Target) const;

	/** Visibility 기반: TraceStart→TraceEnd 사이에 차단 지오메트리가 있으면 true. */
	bool IsTankVisibilityLosBlockedBetween(FVector const& TraceStartWorld, FVector const& TraceEndWorld) const;

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
	// 점프 착지(심작 직후) — 재생은 ABP·애님 시퀀스, 서버는 TankJumpAttackAnimSequence 길이로 종료 타임 설정.
	// -------------------------------------------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|JumpAttack|Setup",
		meta = (AllowPrivateAccess = "true", DisplayName = "Jump Attack After Heartbeat"))
	bool bUseTankJumpAttackAfterHeartbeat = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|JumpAttack|Setup",
		meta = (AllowPrivateAccess = "true",
			ToolTip = "GetPlayLength()로 서버 자동 종료 시간을 잡음. ABP에서는 동일 재생 시간의 Jump 시퀀스를 재생해야 함."))
	TObjectPtr<UAnimSequence> TankJumpAttackAnimSequence;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|JumpAttack|Damage",
		meta = (AllowPrivateAccess = "true", ClampMin = "1.0", DisplayName = "Slam Damage"))
	float TankJumpSlamDamage = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|JumpAttack|Damage",
		meta = (AllowPrivateAccess = "true", ClampMin = "1.0", DisplayName = "Slam Radius (cm)"))
	float TankJumpSlamRadiusCm = 220.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|JumpAttack|Damage",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0",
			DisplayName = "Slam: Max Victim Feet Above Floor (cm)"))
	float TankJumpSlamVictimMaxFeetCmAboveLanding = 95.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|JumpAttack|Arc",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0", DisplayName = "Arc Peak Z Delta (cm)"))
	float TankJumpPeakZDeltaCm = 450.f;

	/** 리프트오프 순간 서버에서 캡슐을 위로 순간 이동(월드 공중 궤적 시작점). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|JumpAttack|Arc",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0", DisplayName = "Lift-off Instant Z (cm)"))
	float TankJumpLiftOffInstantZCm = 120.f;

	/**
	 * 서버 키네마틱 점프 궤적이 지속하는 시간(초). XY·Z 곡선이 이 구간에서 끝난다.
	 * 0 이하면 TankJumpAttackAnimSequence 의 재생 길이를 사용한다(종료 타이머와 동일 근거).
	 * 예전에는 18/30초 고정이라 긴 활공 에셋보다 빨리 바닥에 붙었다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|JumpAttack|Arc",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float TankJumpArcKinematicAirborneSeconds = 0.f;

	/** 상승 구간 비율(미만 = 낙하). 기존 하드코딩은 대략 5/18. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|JumpAttack|Arc",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.01", ClampMax = "0.99"))
	float TankJumpArcAscendFraction = 5.f / 18.f;

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

	/** 이륙 노티 놓치면 점프 시작 후(sec) 곡선 이륙 강제. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|JumpAttack|Timers",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float TankJumpForcedLiftOffAfterMontageStartsSeconds = 0.55f;

	/** 착지 노티 놓치면 점프 시작 후(sec) 착지·슬램 강제. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|JumpAttack|Timers",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float TankJumpForcedLandingAfterMontageStartsSeconds = 1.13333333f;

	UPROPERTY(ReplicatedUsing = OnRep_TankJumpAttackActive)
	bool bTankJumpAttackActive = false;

	/** 서버만 스폰 웨이브 시퀀스 — 복제로 클라 ABP가 Membrane 상태 표시 가능 */
	UPROPERTY(ReplicatedUsing = OnRep_TankMembranePatternCycleActive, VisibleInstanceOnly, BlueprintReadOnly,
		Category = "Enemy|Tank|Membrane", meta = (AllowPrivateAccess = "true"))
	bool bTankMembranePatternCycleActive = false;

	UPROPERTY(Replicated)
	bool bTankJumpShowLandingTelegraph = false;

	UPROPERTY(Replicated)
	FVector TankJumpLandingFloorWorld = FVector::ZeroVector;

	/** bTankJumpAttackActive 복제 시 클라 Idle/Chase 로코 앰비언트가 Jump 까지 따라붙도록 동기화. */
	UFUNCTION()
	void OnRep_TankJumpAttackActive();

	/** bTankMembranePatternCycleActive 복제 — ABP에서 GetEnemyState()==Membrane 동기화 */
	UFUNCTION()
	void OnRep_TankMembranePatternCycleActive();

	FTimerHandle TankJumpFailsafeTimerHandle;
	FTimerHandle TankJumpLiftOffFailsafeTimerHandle;
	FTimerHandle TankJumpLandingFailsafeTimerHandle;
	FTimerHandle TankJumpAnimNaturalEndTimerHandle;

	bool bTankJumpKinematicAscent_Server = false;
	bool bTankJumpLandingDamageCommitted_Server = false;
	double TankJumpLiftOffStampServerSecs = -1.e20;

	FVector TankJumpArcBeginWorld = FVector::ZeroVector;

	/** 심작 종료 직후 점프 패턴. 심작 AoE 밖이어도 TargetActor 유효 시 시전. */
	bool TryBeginTankJumpAttackAfterHeartbeat();
	bool StartTankJumpAttackSequence_Server();
	void CompleteTankJumpAttackSequence_Server();
	void ClearTankJumpTimers();
	void ScheduleTankJumpNaturalEnd_Server();
	void TankJumpAnimNaturalEnd_Server();
	void OnTankJumpFailsafe_Server();
	void TankJumpLiftOffFailsafe_Server();
	void TankJumpLandingFailsafe_Server();

	void TickTankJumpArc_Server();
	void LiftOffJumpAttack_Server_Impl();
	void ApplyJumpSlam_Server(const FVector& LandFloorWorld);

	void UpdateJumpLandingAoEIndicatorVisual();

	static bool TankJumpTraceLandscapeFloor(UWorld* World, AActor* IgnoreActor, AActor* TraceAlsoIgnoreActor,
		FVector WorldProbePt, FVector& OutFloorImpact);
	static float TankJumpResolveFeetCmAboveLandingZ(AActor const* Victim, float LandingFloorWorldZ);

	FVector ComputeJumpLandingFeetProbeWorld_Server() const;

	void SnapshotJumpLandingFloorFromActor_Server(FVector ProbeLocation, AActor* TraceAlsoIgnoreActor);

	// -------------------------------------------------------------------------
	// 양막: 최초 조우 시부터 TankMembraneCooldownSeconds 쿨이 돈다(Next 저장).
	// 발동: 점프 시퀀스 종료 순간만 쿨 검사 후 Membrane 진입.
	// TankMembraneSummon 노티에서 이펙터 웨이브 스폰. 패턴 종료는 웨이브 종료 + TankMembraneFinished 피니시 노티.
	// -------------------------------------------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|Membrane",
		meta = (AllowPrivateAccess = "true"))
	bool bEnableTankMembranePattern = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|Membrane",
		meta = (AllowPrivateAccess = "true", ClampMin = "1.0"))
	float TankMembraneCooldownSeconds = 60.f;

	/**
	 * 발사 튜닝 Override — 0(기본)이면 양막 BP 값을 그대로 사용. 양수면 양막 BP 값을 강제로 덮어쓴다.
	 * 발사 로직 자체(타이머·볼리·자가파괴)는 ATankMembraneEmitterActor 가 담당한다.
	 * 일반적으로는 양막 BP 에서 튜닝하고, Tank 마다 다르게 가고 싶을 때만 여기 값을 세팅한다.
	 */

	/**
	 * true 면 아래 4개 Tank Override 값(부채각/투사체속도/볼리수/볼리간격)을 모두 무시하고
	 * 양막 BP 값을 그대로 쓴다. 옛 BP 에 저장된 override 값을 한 번에 무력화할 수 있어서 디버깅 편함.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|Membrane|Volley",
		meta = (AllowPrivateAccess = "true",
			DisplayName = "양막 BP 값만 사용 (Tank Override 무시)"))
	bool bTankMembraneIgnoreTankOverrides = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|Membrane|Volley",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "89.0",
			EditCondition = "!bTankMembraneIgnoreTankOverrides",
			DisplayName = "Fan Half Angle Deg (0 = 양막 BP 값)"))
	float TankMembraneFanHalfAngleDeg = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|Membrane|Volley",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0",
			EditCondition = "!bTankMembraneIgnoreTankOverrides",
			DisplayName = "Projectile Speed UU (0 = 양막 BP 값)"))
	float TankMembraneProjectileSpeedUU = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|Membrane|Volley",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float TankMembraneProjectileDamage = 10.f;

	/** 양막 1개의 부채꼴(3발) 발사 횟수. 0(기본)이면 양막 BP의 VolleyCount 사용. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|Membrane|Volley",
		meta = (AllowPrivateAccess = "true", ClampMin = "0",
			EditCondition = "!bTankMembraneIgnoreTankOverrides",
			DisplayName = "Volley Count (0 = 양막 BP 값)"))
	int32 TankMembraneVolleyCount = 0;

	/** 볼리 사이 간격(초). 작을수록 공격 속도 빠름. 0(기본)이면 양막 BP의 VolleyIntervalSeconds 사용. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|Membrane|Volley",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0",
			EditCondition = "!bTankMembraneIgnoreTankOverrides",
			DisplayName = "Volley Interval (sec, 0 = 양막 BP 값)"))
	float TankMembraneVolleyIntervalSeconds = 0.f;

	/**
	 * 양막 이펙터가 소환 노티와 같은 틱에 끝나더라도 ABP가 Membrane 상태를 잡을 수 있도록 최소 유지 시간.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|Membrane",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0",
			ToolTip = "Membrane 표시 상태를 최소 이 시간 유지. 0이면 같은 틱에 끝나 애님이 못 잡을 수 있음."))
	float TankMembraneAnimStateHoldSeconds = 0.75f;

	/** 소환 노티를 빠뜨렸을 때 영구 고착을 막는 보호 시간. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|Membrane",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0", DisplayName = "Summon Notify Fail-Safe (sec)"))
	float TankMembraneSummonNotifyFailSafeSeconds = 2.0f;

	/** 이펙터 웨이브는 끝났는데 피니시 노티가 없을 때 패턴 종료까지(0이면 타이머 없음·연출 맡김). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|Membrane",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0",
			DisplayName = "Membrane Finish Notify Fail-Safe (sec)"))
	float TankMembraneFinishNotifyFailSafeSeconds = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|Membrane",
		meta = (AllowPrivateAccess = "true",
			ToolTip = "비어 있으면 패턴이 실행되지 않습니다. ATankMembraneEmitterActor 또는 서브클래스. C++ 기본값은 생성 시 설정."))
	TSubclassOf<ATankMembraneEmitterActor> TankMembraneEmitterClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|Membrane",
		meta = (AllowPrivateAccess = "true",
			ToolTip = "비어 있으면 패턴이 실행되지 않습니다. ATankMembraneProjectile 또는 서브클래스."))
	TSubclassOf<ATankMembraneProjectile> TankMembraneProjectileClass;

	bool bTankMembraneFirstCooldownScheduled = false;
	/** 최초 조우부터 양막 패턴 시간이 도는 기준값(Now >= NextTankMembranePatternTime). */
	double NextTankMembranePatternTime = -1.0;
	/** Membrane 상태 진입 후 소환 노티를 아직 받지 못한 대기 상태. */
	bool bTankMembraneWaitingForSummonNotify = false;

	/** 이펙터 웨이브가 서버에서 모두 종료됨 — 피니시 노티도 필요. */
	bool bTankMembraneWaveEnded_Server = false;

	/** 애님 Tank Membrane Finished 노티 수신(서버). */
	bool bTankMembraneFinishNotifySeen_Server = false;

	int32 TankMembraneEmittersPendingThisWave = 0;
	TArray<TObjectPtr<ATankMembraneEmitterActor>> ActiveTankMembraneEmitters;

	FTimerHandle TankMembraneAnimReleaseTimerHandle;
	FTimerHandle TankMembraneSummonNotifyFailSafeTimerHandle;
	FTimerHandle TankMembraneFinishNotifyFailSafeTimerHandle;

	void ClearTankMembraneAnimReleaseTimer();
	void ClearTankMembraneSummonNotifyFailSafeTimer();
	void ClearTankMembraneFinishNotifyFailSafeTimer();
	void ScheduleTankMembraneFinishFailSafe_Server();

	void TryFinalizeTankMembranePatternDismissal_Server();
	UFUNCTION()
	void FinishTankMembranePatternCycleAnimHold();
	UFUNCTION()
	void OnTankMembraneSummonNotifyFailSafe();
	UFUNCTION()
	void OnTankMembraneFinishNotifyFailSafe();

	void TryPrimeMembraneCooldownAfterFirstAggro(double Now);
	bool IsTankMembraneCooldownReady(double Now) const;
	bool TryStartTankMembranePatternCycle(double Now);
	void StartTankMembranePatternWave();
	void CompleteTankMembranePatternCycle(double Now, bool bApplyAnimStateHold);
	void DestroyActiveTankMembraneEmitters();

	// -------------------------------------------------------------------------
	// 태반 방어 패턴(Pi): 체력 ≤30%(1회)·10초·초당 MaxHealth 2% 회복·플레이어 피격은 태반에만 허용
	//
	// 콘텐츠: Content 경로 예시 BP_TankPlacentaShell = /Game/Enemy/Tank/BP_TankPlacentaShell 을 만들고,
	// ATankEnemy 기본블루프린트 디테일 `Tank Placenta Shell Class` 에 할당하면 시각 메시 등 커스터마이즈 가능.
	// -------------------------------------------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|Placenta",
		meta = (AllowPrivateAccess = "true"))
	bool bEnableTankPlacentaDefensePattern = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|Placenta",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.05", ClampMax = "1.0"))
	float TankPlacentaTriggerHpPercent = 0.30f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|Placenta",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.1"))
	float TankPlacentaDefenseDurationSeconds = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|Placenta",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0"))
	float TankPlacentaHealPerSecondPercentOfMax = 0.02f;

	/** 10초 종료까지 태반이 깨지지 않았을 때 출력 데미지에 곱해지는 증폭(기본 ×1.2 = +20%). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|Placenta",
		meta = (AllowPrivateAccess = "true", ClampMin = "1.0"))
	float TankPlacentaTimedOutOutgoingDamageMultiplier = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|Placenta",
		meta = (AllowPrivateAccess = "true", ClampMin = "10.0"))
	float TankPlacentaShellRadiusCm = 260.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|Placenta",
		meta = (AllowPrivateAccess = "true", ClampMin = "1.0"))
	float TankPlacentaShellMaxHealth = 800.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Tank|Placenta",
		meta = (AllowPrivateAccess = "true"))
	TSubclassOf<ATankPlacentaShellActor> TankPlacentaShellActorClass;

	UPROPERTY(ReplicatedUsing = OnRep_TankPlacentaDefenseActive, VisibleInstanceOnly, BlueprintReadOnly,
		Category = "Enemy|Tank|Placenta", meta = (AllowPrivateAccess = "true"))
	bool bTankPlacentaDefenseActive = false;

	UFUNCTION()
	void OnRep_TankPlacentaDefenseActive();

	TObjectPtr<ATankPlacentaShellActor> ActiveTankPlacentaShell = nullptr;

	/** 패턴 시작 여부 평생 1회(서버 플래그). */
	bool bTankPlacentaDefenseConsumed_Server = false;

	/** 패턴 활성 출력 데미지 증폭(서버, 하트비트 디스패치 등에 반영). */
	float TankOutgoingDamageMultiplierRuntime = 1.f;

	int32 TankPlacentaHealTicksRemaining_Server = 0;

	FTimerHandle TankPlacentaDurationTimerHandle;
	FTimerHandle TankPlacentaHealTimerHandle;

	void ClearTankPlacentaDefenseTimers_Server();
	void TryTankPlacentaDefenseAfterIncomingDamage_Server();

	bool DoesTankHaveAlivePlacentaShell_Server() const;
	bool ShouldSuppressTankIncomingDamageFromCauseForPlacenta(AController const* EventInstigator,
		AActor const* DamageCauser) const;

	void NotifyStickyAggroIfPlayerDamagedBeyondRange(float AppliedDamage, AController const* EventInstigator,
		AActor const* DamageCauser) override;

	void StartTankPlacentaDefensePattern_Server();

	void TickTankPlacentaHeal_Server();
	void FinishTankPlacentaDefense_Server(bool bTimedOutWithShellAlive);
	void OnTankPlacentaDefenseDurationExpire_Server();

	void DestroyTankPlacentaShellIfAny_Server();

	bool bBossHPShown = false;
	
	void TryShowBossHUD();
	void RefreshBossHUD();
	void HideBossHUD();
};
