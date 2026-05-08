#pragma once

// =============================================================================
// AEnemyBase — 모든 적 캐릭터의 공통 부모.
//
// FSM(의사결정): Idle → Chase / Attack → (어그로 없음 시) Investigate → Search → Patrol → Idle
//   · ReportStimulus: 외부 자극 위치 + EEnemyStimulusType(어그로 없을 때 Investigate, 타입은 연출·BP 분기용)
//   · TrackLight: 콘 안 + 적 정면에 빛(기본)일 때만 스포트 위치 추적; 손전등만 꺼지면 sealed 후 마지막 점까지(어그로 시 Chase 우선)
//   · AggroRadius: UE 단위 cm (1000 ≈ 10m). 0이면 거리 무시 항상 추격. 타겟은 GetPlayerPawn(0)
//
// 전투(외부): TakeDamage 적용 → CurrentHealth 차감 → OnEnemyDamaged → 0이하면 Die().
//   PerformAttack 근접 판단은 OnEnemyAttackCommitted 만 발행(실 데미지/연출은 전투 측).
//   · OnLightHit 도 이벤트만 발행(특수 AI용·Luxeater 흡수). 빛으로 슬로우/스턴/사망은 전투 측이 ApplyCC*/TakeDamage로 처리.
//
// CC(기술·아이템 등): EEnemyCCState — Slow(이속 배율), Stun(경직·행동 정지).
//   · Duration<=0 이면 타이머 없이 유지 → ClearCCSlow / ClearCCStun 으로 해제
//
// NavMesh가 있어야 MoveToActor / MoveToLocation 동작. 레벨에 Nav Mesh Bounds 권장.
// 비어그로 Idle: PatrolPoints 없을 때 IdleWander로 주변 배회(옵션).
// =============================================================================

#include "OblivioComponents/CombatInterface.h"

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "EnemyBase.generated.h"

class AEnemyBase;
class USpotLightComponent;
class UEnemyCombatComponent;

/** 적 행동 상태(FSM). Stunned는 bCCStunned 플래그를 ABP에서 읽기 위한 래핑 상태. */
UENUM(BlueprintType)
enum class EEnemyAIState : uint8
{
	Idle UMETA(DisplayName = "Idle"),
	Chase UMETA(DisplayName = "Chase"),
	Attack UMETA(DisplayName = "Attack"),
	/** 손전등 앞면 조명 추적: 목표는 월드 점, 손전등 끔/이탈 시 마지막 점까지만 이동(Chase와 분리). */
	TrackLight UMETA(DisplayName = "TrackLight"),
	Patrol UMETA(DisplayName = "Patrol"),
	Investigate UMETA(DisplayName = "Investigate"),
	Search UMETA(DisplayName = "Search"),
	/** CC 경직 중. 내부 FSM 상태는 유지, ABP 전용 표현 상태. */
	Stunned UMETA(DisplayName = "Stunned"),
	Dead UMETA(DisplayName = "Dead")
};

/** 이동 저하·경직 등 CC(FSM과 별개). GetCrowdControlState는 빛 둔화/정지도 함께 반영. */
UENUM(BlueprintType)
enum class EEnemyCCState : uint8
{
	None UMETA(DisplayName = "None"),
	Slowed UMETA(DisplayName = "Slowed"),
	Stunned UMETA(DisplayName = "Stunned"),
};

/** ReportStimulus에 넘기는 자극 종류. FSM 우선순위는 동일(어그로 > Investigate). */
UENUM(BlueprintType)
enum class EEnemyStimulusType : uint8
{
	Noise UMETA(DisplayName = "Noise"),
	ThrownItem UMETA(DisplayName = "ThrownItem"),
	Distraction UMETA(DisplayName = "Distraction"),
	Custom UMETA(DisplayName = "Custom"),
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEnemyDiedSignature, AEnemyBase*, Enemy);
/** TakeDamage 적용 후: (실제 들어간 데미지, 차감 직후 현재 체력, 최대체력). PerformAttack 브로드캐스트와 별개. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FEnemyDamagedSignature, float, DamageAmount, float, CurrentHealth, float, MaxHealth);

/** FSM 전이 시(Dead 포함). BP에서 전투·연출 테스트용. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FEnemyFSMStateChangedSignature, AEnemyBase*, Enemy, EEnemyAIState, OldState, EEnemyAIState, NewState);
/** 근접 공격 가능 판단이 성립했을 때. 실제 피해/상태/연출 처리는 전투 시스템에서 담당. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FEnemyAttackCommittedSignature, AEnemyBase*, Enemy, AActor*, Target, float, DamageAmount);
/** ReportStimulus가 Investigate 큐에 반영됐을 때(어그로 있으면 호출 안 됨). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FEnemyStimulusReportedSignature, AEnemyBase*, Enemy, FVector, StimulusLocation, EEnemyStimulusType, StimulusType);
/** TrackLight 진입 true / 이탈 false. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEnemyTrackLightPhaseSignature, AEnemyBase*, Enemy, bool, bEnteredTrackLight);
/** SetTargetActor 호출 시마다(전투 타겟 바인딩 테스트용). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEnemyTargetChangedSignature, AEnemyBase*, Enemy, AActor*, NewTarget);
/** OnLightHit 진입 시(노출·둔화 연출·테스트용). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FEnemyLightHitSignature, AEnemyBase*, Enemy, float, Intensity, float, Duration);

UCLASS(Blueprintable)
class OBLIVIO_API AEnemyBase : public ACharacter, public ICombatInterface//전투 컴포넌트용 인터페이스 추가 상속
{
	GENERATED_BODY()

public:
	AEnemyBase();

	// 매 틱: 빛 노출 감쇠 → 타겟 재탐색 → FSM 갱신 → 현재 상태 실행(이동/공격/패트롤 등)
	virtual void Tick(float DeltaSeconds) override;
	// 엔진 damage 파이프라인. 부모 처리 후 현재 적 체력 반영 및 OnEnemyDamaged, 사망 시 Die().
	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	/** 스턴 중이면 Stunned를 반환. ABP 스테이트 머신에서 경직 애니메이션 분기에 사용. */
	UFUNCTION(BlueprintCallable, Category = "Enemy|State")
	EEnemyAIState GetEnemyState() const { return bCCStunned ? EEnemyAIState::Stunned : EnemyState; }

	UFUNCTION(BlueprintCallable, Category = "Enemy|State")
	bool IsAlive() const override { return EnemyState != EEnemyAIState::Dead && CurrentHealth > 0.0f; } //인터페이스 오버라이드 키워드 추가

	UFUNCTION(BlueprintPure, Category = "Enemy|CrowdControl")
	EEnemyCCState GetCrowdControlState() const;

	/** SpeedMultiplier: 1=정상, 0.5=절반 이속. Duration<=0 이면 ClearCCSlow 할 때까지 유지. */
	UFUNCTION(BlueprintCallable, Category = "Enemy|CrowdControl")
	void ApplyCCSlow(float SpeedMultiplier, float Duration = 0.0f) override;	//인터페이스 오버라이드 키워드 추가

	/** 경직: AI/추격·공격 중단. Duration<=0 이면 ClearCCStun 할 때까지. */
	UFUNCTION(BlueprintCallable, Category = "Enemy|CrowdControl")
	void ApplyCCStun(float Duration = 0.0f) override;	//인터페이스 오버라이드 키워드 추가

	UFUNCTION(BlueprintCallable, Category = "Enemy|CrowdControl")
	void ClearCCSlow();

	UFUNCTION(BlueprintCallable, Category = "Enemy|CrowdControl")
	void ClearCCStun();

	UFUNCTION(BlueprintPure, Category = "Enemy|CrowdControl")
	bool IsCCStunned() const { return bCCStunned; }

	UFUNCTION(BlueprintPure, Category = "Enemy|Damage")
	bool WasLastDamageFromLight() const { return bLastDamageWasLight; }

	UFUNCTION(BlueprintCallable, Category = "Enemy|Light")
	virtual void OnLightHit(float Intensity, float Duration);

	UFUNCTION(BlueprintPure, Category = "Enemy|Light|CC")
	virtual float GetLightExposureAccum() const { return LightExposureAccum; }

	UFUNCTION(BlueprintCallable, Category = "Enemy|Target")
	void SetTargetActor(AActor* NewTarget);

	UFUNCTION(BlueprintPure, Category = "Enemy|Target")
	AActor* GetTargetActor() const { return TargetActor; }

	UFUNCTION(BlueprintPure, Category = "Enemy|Combat")
	float GetAttackDamage() const { return AttackDamage; }

	UFUNCTION(BlueprintPure, Category = "Enemy|Combat")
	float GetAttackRange() const { return AttackRange; }

	UFUNCTION(BlueprintPure, Category = "Enemy|Combat")
	float GetAttackCooldown() const { return AttackCooldown; }

	UFUNCTION(BlueprintCallable, Category = "Enemy|Combat")
	void SetAttackDamage(float NewDamage) { AttackDamage = FMath::Max(0.f, NewDamage); }

	/** 회복(양수). 0 이하 무시. UI/회복 스킬용. OnEnemyDamaged 로 음수 데미지 형태 브로드캐스트 가능하나 여기선 별도. */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Stats")
	void Heal(float Amount);

	/** 체력 강제 설정(0~MaxHealth 클램프). 0이면 Die() 호출. 전투 시스템이 단일 진실원천일 때 사용. */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Stats")
	void SetCurrentHealth(float NewHealth);

	/** Die() 직접 호출 (전투 측 즉살/연출용). 이미 Dead면 무시. */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Stats")
	void KillEnemy();

	/**
	 * 자극 위치 보고. 어그로가 없을 때만 Investigate 큐에 넣음(플레이어 추적보다 우선순위 낮음은 기존과 동일).
	 * StimulusType은 애니·VFX·BP 분기용.
	 */
	UFUNCTION(BlueprintCallable, Category = "Enemy|FSM", meta = (AdvancedDisplay = "StimulusType"))
	void ReportStimulus(FVector WorldLocation, EEnemyStimulusType StimulusType = EEnemyStimulusType::Noise);

	UFUNCTION(BlueprintPure, Category = "Enemy|FSM")
	EEnemyStimulusType GetLastReportedStimulusType() const { return LastReportedStimulusType; }
	
	UFUNCTION(BlueprintPure, Category = "Enemy|Stats")
	float GetCurrentHealthForUI() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category = "Enemy|Stats")
	float GetMaxHealthForUI() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category = "Enemy|Stats")
	float GetHealthPercent() const { return MaxHealth > 0.0f ? CurrentHealth / MaxHealth : 0.0f; }

	/** 적 SFX 볼륨 배율(0=무음, 1=기본). BP Class Defaults 또는 옵션에서 SetEnemySoundVolumeMultiplier. */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Audio")
	void SetEnemySoundVolumeMultiplier(float NewMultiplier);

	UFUNCTION(BlueprintPure, Category = "Enemy|Audio")
	float GetEnemySoundVolumeMultiplier() const { return EnemySoundVolumeMultiplier; }

	UPROPERTY(BlueprintAssignable, Category = "Enemy|Events")
	FEnemyDiedSignature OnEnemyDied;

	UPROPERTY(BlueprintAssignable, Category = "Enemy|Events")
	FEnemyDamagedSignature OnEnemyDamaged;

	UPROPERTY(BlueprintAssignable, Category = "Enemy|Events|Combat")
	FEnemyFSMStateChangedSignature OnEnemyFSMStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Enemy|Events|Combat")
	FEnemyAttackCommittedSignature OnEnemyAttackCommitted;

	UPROPERTY(BlueprintAssignable, Category = "Enemy|Events|Combat")
	FEnemyStimulusReportedSignature OnEnemyStimulusReported;

	UPROPERTY(BlueprintAssignable, Category = "Enemy|Events|Combat")
	FEnemyTrackLightPhaseSignature OnEnemyTrackLightPhase;

	UPROPERTY(BlueprintAssignable, Category = "Enemy|Events|Combat")
	FEnemyTargetChangedSignature OnEnemyTargetChanged;

	UPROPERTY(BlueprintAssignable, Category = "Enemy|Events|Combat")
	FEnemyLightHitSignature OnEnemyLightHit;

	//전투 컴포넌트용 인터페이스 함수
	void ApplyHealth(float Damage) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crafting")
	TObjectPtr<UEnemyCombatComponent> CombatComp;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Stats", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Stats")
	float CurrentHealth = 100.0f;

	/** 배회·Patrol·Search·Investigate 등 비추격 이동 기준 이속(cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Stats", meta = (ClampMin = "0.0"))
	float MoveSpeed = 350.0f;

	/** Chase·Attack 시 이속. 0이면 MoveSpeed와 동일. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Stats", meta = (ClampMin = "0.0"))
	float ChaseMoveSpeed = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Combat", meta = (ClampMin = "0.0"))
	float AttackDamage = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Combat", meta = (ClampMin = "1.0"))
	float AttackRange = 150.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Combat", meta = (ClampMin = "0.1"))
	float AttackCooldown = 1.0f;

	/** MoveToActor 도착 판정. AttackRange보다 크면 추격이 먼저 멈춰 Chase→Attack 전환이 안 될 수 있음(UpdateChase에서 자동으로 AttackRange보다 안쪽으로 제한). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Navigation", meta = (ClampMin = "1.0"))
	float ChaseAcceptanceRadius = 80.0f;

	/** 추격 수용 반경 상한 = AttackRange - 이 값(cm). 커질수록 더 가까이 붙인 뒤 Chase 종료 → 공격 전환 안정. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Navigation", meta = (ClampMin = "1.0", ClampMax = "500.0"))
	float ChaseProximityBuffer = 40.0f;

	/**
	 * 타겟이 이 거리(cm) 이상 이동해야 Chase 경로를 재요청한다.
	 * 매 프레임 MoveToActor를 호출하면 진행 중인 경로가 중단·재시작되어 적이 제자리에 멈추는 현상이 생기므로,
	 * 이미 이동 중일 때는 타겟이 충분히 움직인 경우에만 재요청한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Navigation", meta = (ClampMin = "0.0"))
	float ChasePathRefreshDistance = 120.0f;

	/**
	 * false를 반환하면 Chase 중 막힘 감지·NavMesh 복구를 건너뛴다.
	 * 벽을 통과하는 Ghost형 적에서 오버라이드해 사용한다.
	 */
	virtual bool IsStuckRecoveryEnabled() const { return true; }

	/**
	 * false를 반환하면 플레이어 방향 장애물 감지·공격을 건너뛴다.
	 * 벽을 통과하는 Ghost형 적(ScreamEnemy 등)에서 오버라이드해 사용한다.
	 */
	virtual bool IsObstacleAttackEnabled() const { return true; }

	/**
	 * false를 반환하면 ReportStimulus(소리 자극)를 무시하고 Investigate 상태로 전환하지 않는다.
	 * 소리에 반응하지 않아야 하는 적(HeadlessLoverEnemy 등)에서 오버라이드해 사용한다.
	 */
	virtual bool IsSoundInvestigationEnabled() const { return true; }

	/** 에너미→플레이어 라인트레이스 주기(초). 장애물 감지에 사용. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Navigation", meta = (ClampMin = "0.1"))
	float ObstacleScanInterval = 0.5f;

	/** Chase 중 이 간격(초)마다 '막힘' 여부를 검사한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Navigation", meta = (ClampMin = "0.2"))
	float StuckCheckInterval = 1.2f;

	/** 체크 간격 동안 이 거리(cm) 이하로 이동하면 막힘으로 판단한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Navigation", meta = (ClampMin = "1.0"))
	float StuckDistanceThreshold = 30.0f;

	/** 연속 막힘 판정 N회 시 우회 NavMesh 지점으로 이동을 시도한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Navigation", meta = (ClampMin = "1"))
	int32 StuckCountThreshold = 2;

	/** 우회 지점 탐색 반경(cm). 좌·우·앞-대각 방향으로 이 거리 안의 NavMesh 위 지점을 찾는다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Navigation", meta = (ClampMin = "50.0"))
	float StuckRecoveryRadius = 300.0f;

	/** 0이면 플레이어가 있으면 항상 어그로. 0보다 크면 이 거리(cm) 밖은 추격 해제. BP·레벨 액터에서 조정 가능. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|FSM", meta = (ClampMin = "0.0"))
	float AggroRadius = 0.0f;

	/** true면 어그로 거리를 XY(수평)만으로 계산. 층 높이 차로 안 닿는 것처럼 보일 때 사용. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|FSM")
	bool bAggroUseHorizontalDistance = true;

	/** PIE/게임에서 어그로 반경 구 디버그(녹=플레이어 인식, 주황=밖). Shipping 빌드에서는 무시. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Debug")
	bool bDebugDrawAggroRadius = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|FSM", meta = (ClampMin = "1.0"))
	float PatrolAcceptanceRadius = 80.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|FSM", meta = (ClampMin = "1.0"))
	float InvestigateAcceptanceRadius = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|FSM", meta = (ClampMin = "0.0"))
	float InvestigateStimulusTimeout = 12.0f;

	/** 어그로를 잃은 뒤 탐색 시간(초). 0이면 Search 비활성. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|FSM", meta = (ClampMin = "0.0"))
	float SearchPhaseDuration = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|FSM", meta = (ClampMin = "1.0"))
	float SearchRadius = 450.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|FSM", meta = (ClampMin = "0.1"))
	float SearchRetargetInterval = 1.5f;

	/** 순찰 지점(빈 액터 등 배치 후 끌어다 놓기). 비어 있으면 Patrol 상태 미사용. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|FSM")
	TArray<TObjectPtr<AActor>> PatrolPoints;

	/** 어그로 밖·Idle이고 Patrol 지점이 없을 때 주변 Nav로 배회. 끄면 제자리 정지(기존 동작). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Idle")
	bool bEnableIdleWander = true;

	/** Idle 배회: 스폰 위치 기준 수평 반경(cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Idle", meta = (ClampMin = "50.0"))
	float IdleWanderRadius = 450.0f;

	/** 새 목표까지 대기 시간(초). 도착 전이면 경로 유지. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Idle", meta = (ClampMin = "0.5"))
	float IdleWanderRetargetInterval = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Idle", meta = (ClampMin = "1.0"))
	float IdleWanderAcceptanceRadius = 64.0f;

	// ------------------------------------------------------------------
	// 빛 CC (Light Crowd Control)
	// ------------------------------------------------------------------

	/**
	 * 빛 경직 발동에 필요한 누적 노출 시간(초).
	 * 0이면 빛에 닿는 즉시 경직.  > 0이면 이 초만큼 빛을 맞아야 경직 발동.
	 * ScreamEnemy는 자체 LightStunBuildupSeconds를 가지므로 이 값을 사용하지 않는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Light|CC", meta = (ClampMin = "0.0"))
	float LightStunBuildupSeconds = 0.0f;

	/**
	 * 빛 경직 지속 시간(초).
	 * 0이면 CombatComp→StunDuration 값을 사용한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Light|CC", meta = (ClampMin = "0.0"))
	float LightStunDuration = 0.0f;

	// ------------------------------------------------------------------
	// Light Track (손전등 추적)
	// ------------------------------------------------------------------

	/** true면 플레이어 손전등(앞면+콘) 조건일 때 TrackLight FSM 사용. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Light|Track")
	bool bEnableLightTracking = true;

	/** 손전등 콘·앞면 판정에 쓰는 몸 높이 오프셋(cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Light|Track", meta = (ClampMin = "0.0"))
	float LightTrackConeTestZ = 40.0f;

	/** 콘 판정 시 AttenuationRadius에 더하는 여유(cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Light|Track", meta = (ClampMin = "0.0"))
	float LightTrackConeRadiusSlack = 32.0f;

	/**
	 * true면 적 정면에 손전등이 올 때만 추적(적 Forward · (적→스포트) Dot ≥ LightTrackFrontFaceMinDot).
	 * false면 콘 안이면 등·옆에서 비춰도 추적(탑다운 등).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Light|Track")
	bool bLightTrackRequireFrontFace = true;

	/** 정면 판정: 1에 가까울수록 좁은 정면(직면). 0.25 ≈ 약 75° 이내. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Light|Track", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float LightTrackFrontFaceMinDot = 0.35f;

	/** TrackLight MoveTo 도착 반경(cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Light|Track", meta = (ClampMin = "1.0"))
	float LightTrackAcceptanceRadius = 72.0f;

	/**
	 * 콘/정면 판정이 일시적으로 빠질 때 즉시 Idle로 떨어지지 않도록 유지하는 시간(초).
	 * 0이면 기존 동작과 동일(빠지면 즉시 Idle). 0.3~0.6 권장.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Light|Track", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float LightTrackLossGracePeriod = 0.45f;

	/** 리얼·루프 등 적 오디오 총괄 배율. 파생 클래스는 ApplyEnemySoundVolumes에서 컴포넌트에 반영. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Audio", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float EnemySoundVolumeMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Death", meta = (ClampMin = "0.0"))
	float CorpseLifeSpan = 3.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|State")
	EEnemyAIState EnemyState = EEnemyAIState::Idle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Target")
	TObjectPtr<AActor> TargetActor;

	/** 근접 공격 판단 시 BP/C++에서 실제 공격 방식을 구현. 기본은 OnEnemyAttackCommitted 브로드캐스트만 수행. */
	UFUNCTION(BlueprintNativeEvent, Category = "Enemy|Combat")
	void PerformAttack(AActor* Target);
	virtual void PerformAttack_Implementation(AActor* Target);

	/** 기본 타겟: 월드 플레이어 0번 캐릭터 */
	virtual void FindDefaultTarget();
	/** FSM 전이만 담당(어그로·자극·패트롤 여부). 실제 이동은 Tick 스위치에서 */
	virtual void UpdateState();
	virtual void UpdateChase();
	virtual void UpdateAttack();

	/** Chase 중 주기적 막힘 감지. StuckCountThreshold 회 연속 감지 시 TryRecoverFromStuck 호출. */
	void CheckAndRecoverFromStuck(float DeltaSeconds);
	/** NavMesh 위 우회 지점(좌·우·앞-대각)으로 임시 이동해 막힘에서 벗어난다. 후보 없으면 수용반경 확대 재요청. */
	void TryRecoverFromStuck();

	/**
	 * Chase 상태에서 주기적으로 에너미→플레이어 사이 장애물을 감지하고,
	 * 장애물이 있으면 직접 접근·공격·파괴 처리를 수행한다.
	 */
	void HandleBlockingObstacle(float DeltaSeconds);
	virtual void UpdatePatrol(float DeltaSeconds);
	virtual void UpdateInvestigate(float DeltaSeconds);
	virtual void UpdateSearch(float DeltaSeconds);
	virtual void UpdateIdle(float DeltaSeconds);
	virtual void UpdateTrackLight(float DeltaSeconds);
public:
	virtual void Die();
protected:

	void SetEnemyState(EEnemyAIState NewState);

	/** SetEnemyState에서 실제로 바뀐 직후 호출(Old→New). 기본 빈 구현. */
	virtual void NotifyEnemyStateChanged(EEnemyAIState OldState, EEnemyAIState NewState) {}

	/** TakeDamage로 CurrentHealth를 차감한 직후(사망 처리 전). 보스 페이즈 갱신 등에 사용. */
	virtual void NotifyEnemyDamageApplied(float AppliedDamage);
	bool IsTargetInAttackRange() const;
	/** AggroRadius 내(또는 0이면 무한)일 때 true. 보스 등은 “한 번 들어오면 영구 추격”용으로 오버라이드 가능. */
	virtual bool HasValidAggroTarget() const;
	void StopEnemyMovement();

	void RefreshWalkSpeedFromSources();
	/** Chase·Attack vs 그 외 이동 기준 이속. 파생 클래스에서 절름발이 추격자 등 이단 속도용 오버라이드. */
	virtual float GetLocomotionBaseSpeed() const;
	void OnCCSlowExpired();
	void OnCCStunExpired();
	void DrawAggroDebug();

	/** SetEnemySoundVolumeMultiplier 이후 호출 — 스토커 등 오디오 컴포넌트 동기화용. */
	virtual void ApplyEnemySoundVolumes();

private:
	float LastAttackTime = -BIG_NUMBER;

	bool bCCSlowActive = false;
	float CCSlowSpeedMultiplier = 1.0f;
	FTimerHandle CCSlowTimerHandle;

	bool bCCStunned = false;
	FTimerHandle CCStunTimerHandle;

	bool bLastDamageWasLight = false;

	bool bHadAggroLastTick = false;
	/** 마지막으로 플레이어를 어그로로 본 월드 위치(Search 앵커용) */
	FVector LastKnownTargetLocation = FVector::ZeroVector;
	int32 CurrentPatrolIndex = 0;

	/** 마지막 MoveToActor 요청 시 타겟 위치. 경로 재요청 여부 판단에 사용. */
	FVector LastChaseRequestedTargetPos = FVector::ZeroVector;
	/** 마지막 막힘 체크 시 자신의 위치. */
	FVector LastStuckCheckLocation = FVector::ZeroVector;
	/** 막힘 체크 경과 시간(초). */
	float StuckCheckTimer = 0.0f;
	/** 연속 막힘 판정 횟수. */
	int32 StuckCounter = 0;

	bool bHasPendingInvestigate = false;
	FVector PendingInvestigateLocation = FVector::ZeroVector;
	float InvestigateTimerRemaining = 0.0f;
	/** 마지막으로 큐에 올라간 ReportStimulus의 타입(어그로로 무시된 보고는 갱신 안 함). */
	EEnemyStimulusType LastReportedStimulusType = EEnemyStimulusType::Noise;

	float SearchTimeRemaining = 0.0f;
	FVector SearchAnchor = FVector::ZeroVector;
	float SearchRetargetCooldown = 0.0f;

	float IdleWanderRetargetCooldown = 0.0f;

	/** 빛 경직 누적 노출 시간(초). LightStunBuildupSeconds에 도달하면 경직 발동 후 리셋. */
	float LightExposureAccum = 0.0f;

	/** 현재 경로를 막고 있는 플레이어 설치 장애물. nullptr이면 비활성. */
	TObjectPtr<AActor> BlockingObstacle;
	/** 장애물 스캔 누적 시간(초). */
	float ObstacleScanTimer = 0.0f;
	/** 장애물 공격 쿨타임 누적(초). */
	float ObstacleAttackTimer = 0.0f;

	bool bLightTrackGoalValid = false;
	bool bLightTrackSealed = false;
	FVector LightTrackGoalWorld = FVector::ZeroVector;
	/** 손전등 콘/정면 판정이 잠깐 실패해도 LastGoal까지 잠시 더 따라가게 해주는 잔여시간. */
	float LightTrackGraceRemaining = 0.0f;

	void ClearLightTrackState();
	bool TryComputeFlashlightTrackGoal(FVector& OutGoal);

	/** 플레이어(또는 타겟) 손전등 USpotLight. 없으면 nullptr. */
	USpotLightComponent* ResolveFlashlightSpotForTracking() const;

	/** 손전등 꺼짐·배터리 등으로 “추적 불가(마지막 점까지)”일 때만 true — 콘 밖은 false. */
	bool IsFlashlightTrackSourceOff(USpotLightComponent* Spot) const;

	/** bLightTrackRequireFrontFace일 때: 적 Forward와 (적→LightWorld) Dot 검사. 꺼져 있으면 항상 true. */
	bool PassesLightTrackFrontFaceTest(const FVector& LightWorldLocation) const;
};
