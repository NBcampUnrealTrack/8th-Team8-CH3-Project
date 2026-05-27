#pragma once

// =============================================================================
// AStagingEnemy — 시작 방 등 연출 전용 적. 플레이어에게 데미지를 주지 않습니다.
// AnimNotify(EStagingEnemyCinematicNotify) + 플레이어 몽타주(EPlayerCinematicNotify)로
// 붙잡기 → 대치 → 밀치기 → 넘어짐 → 손전등 ON → 빛 데미지 → 사망 연출을 구성합니다.
// =============================================================================

#include "CoreMinimal.h"
#include "AIEnemy/EnemyBase.h"
#include "Cinematic/StagingCinematicTypes.h"
#include "Math/Color.h"
#include "StagingEnemy.generated.h"

class AOblivioCharacter;
class UAnimSequence;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FStagingEnemyCinematicStateChanged,
	AStagingEnemy*, Enemy, EStagingEnemyCinematicState, NewState);

UCLASS(Blueprintable)
class OBLIVIO_API AStagingEnemy : public AEnemyBase
{
	GENERATED_BODY()

public:
	AStagingEnemy();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	virtual EEnemyAIState GetEnemyState() const override;

	virtual void CommitAttackFromAnimNotify(AActor* OptionalTargetOverride = nullptr) override;
	virtual void DispatchEnemyAttackCommitted(AActor* Target, float DamageAmount,
		TSubclassOf<UDamageType> DamageTypeClass) override;

	virtual void Die() override;

	virtual void ApplyCCStun(float Duration = 0.0f) override;
	virtual void ApplyCCSlow(float SpeedMultiplier, float Duration = 0.0f) override;
	virtual void OnLightHit(float Intensity, float Duration) override;

	virtual bool IsTargetInAttackRange() const override;

	virtual EEnemyAIState SelectStateWhileAggroed() const override;

	virtual void UpdateState() override;

	UFUNCTION(BlueprintCallable, Category = "Staging|Cinematic")
	void StartOpeningCinematic(AOblivioCharacter* Player);

	UFUNCTION(BlueprintCallable, Category = "Staging|Cinematic")
	void SetStagingState(EStagingEnemyCinematicState NewState);

	UFUNCTION(BlueprintPure, Category = "Staging|Cinematic")
	EStagingEnemyCinematicState GetStagingState() const { return StagingState; }

	UFUNCTION(BlueprintCallable, Category = "Staging|Cinematic")
	void HandleStagingCinematicNotify(EStagingEnemyCinematicNotify NotifyEvent);

	/** 플레이어 몽타주 Execute Auto Push → ABP Knockdown 전환 + 넉백. */
	UFUNCTION(BlueprintCallable, Category = "Staging|Cinematic")
	void HandlePlayerExecuteAutoPush();

	/** 플레이어 몽타주 Force Flashlight On → ABP Dead 전환 + 손전등 + 사망 처리. */
	UFUNCTION(BlueprintCallable, Category = "Staging|Cinematic")
	void HandlePlayerForceFlashlightOn();

	/** (에너미 몽타주 노티용) 자동 밀치기. */
	UFUNCTION(BlueprintCallable, Category = "Staging|Cinematic")
	void ExecuteAutoPush();

	UFUNCTION(BlueprintPure, Category = "Staging|Anim")
	bool IsApproachingForGrab() const;

	UFUNCTION(BlueprintPure, Category = "Staging|Anim")
	virtual bool ShouldPlayGrabAnimation() const;

	UFUNCTION(BlueprintPure, Category = "Staging|Anim")
	virtual bool ShouldPlayKnockdownAnimation() const;

	/** ABP Knockdown 유지 — 오프닝 LS 종료 후 true, 손전등 획득 시 false. */
	UFUNCTION(BlueprintPure, Category = "Staging|Anim")
	bool ShouldRemainInKnockdownPose() const;

	/** E 연타 성공 넉백 구간(CabinetEnemy). ABP 전환용. */
	UFUNCTION(BlueprintPure, Category = "Staging|Anim")
	virtual bool ShouldPlayMashKnockbackAnimation() const { return false; }

	UFUNCTION(BlueprintPure, Category = "Staging|Anim")
	bool ShouldPlayDeadAnimation() const;

	UFUNCTION(BlueprintPure, Category = "Staging|Anim")
	float GetDistanceToLinkedPlayer() const;

	UFUNCTION(BlueprintCallable, Category = "Staging|Cinematic")
	void ForceFlashlightOnPlayer();

	UFUNCTION(BlueprintCallable, Category = "Staging|Cinematic")
	void ApplyCinematicLightDamage();

	UFUNCTION(BlueprintCallable, Category = "Staging|Cinematic")
	void FinishCinematicDeath();

	/** Level Sequence 재생 전 ABP 해제(Sequencer가 스켈레톤 제어). */
	UFUNCTION(BlueprintCallable, Category = "Staging|LevelSequence")
	void PrepareForLevelSequencePlayback();

	/** 시퀀스 재생 실패/중단 시 Prepare 롤백. */
	UFUNCTION(BlueprintCallable, Category = "Staging|LevelSequence")
	void RestoreAfterLevelSequenceAbort();

	/** 시퀀스 정상 종료 후 BP/AnimSequence 애니 복구. */
	UFUNCTION(BlueprintCallable, Category = "Staging|LevelSequence")
	void RestoreAfterLevelSequenceFinished();

	UFUNCTION(BlueprintCallable, Category = "Staging|LevelSequence", meta = (WorldContext = "WorldContextObject"))
	static void PrepareAllForLevelSequence(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Staging|LevelSequence", meta = (WorldContext = "WorldContextObject"))
	static void RestoreAllAfterLevelSequenceAbort(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Staging|LevelSequence", meta = (WorldContext = "WorldContextObject"))
	static void RestoreAllAfterLevelSequenceFinished(const UObject* WorldContextObject);

	/** 플레이어가 월드 손전등을 획득한 뒤 연출 적을 전투 AI(추격·공격)로 전환. */
	UFUNCTION(BlueprintCallable, Category = "Staging|Combat")
	void ActivatePostFlashlightPickupCombat(AOblivioCharacter* Player);

	UFUNCTION(BlueprintCallable, Category = "Staging|Combat", meta = (WorldContext = "WorldContextObject"))
	static void ActivateAllAfterFlashlightPickup(const UObject* WorldContextObject, AOblivioCharacter* Player);

	/** GameInstance에 처치 기록된 연출 적을 월드에서 제거(GI 준비 지연 대비). */
	UFUNCTION(BlueprintCallable, Category = "Staging|Persistence", meta = (WorldContext = "WorldContextObject"))
	static void DestroyAllDefeatedInWorld(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Staging|Combat")
	bool IsPostFlashlightPickupCombatActive() const { return bPostFlashlightPickupCombatActive; }

	UFUNCTION(BlueprintPure, Category = "Staging|Cinematic")
	AOblivioCharacter* GetLinkedPlayer() const { return LinkedPlayer.Get(); }

	UFUNCTION(BlueprintPure, Category = "Staging|Debug")
	bool IsStagingDebugEnabled() const { return bDebugStagingCinematic; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Staging|Debug")
	bool bDebugStagingCinematic = true;

	UPROPERTY(BlueprintAssignable, Category = "Staging|Cinematic")
	FStagingEnemyCinematicStateChanged OnStagingCinematicStateChanged;

	/** BP에서 넘어짐·사망 몽타주 재생 등 추가 연출용. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Staging|Cinematic")
	void OnStagingCinematicNotify(EStagingEnemyCinematicNotify NotifyEvent);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Staging|Cinematic")
	EStagingEnemyCinematicState StagingState = EStagingEnemyCinematicState::Idle;

	UPROPERTY(Transient)
	TWeakObjectPtr<AOblivioCharacter> LinkedPlayer;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Staging|Cinematic")
	bool bAutoStartOpeningCinematic = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Staging|Cinematic", meta = (ClampMin = "0.0"))
	float AutoStartDelaySeconds = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Staging|Cinematic", meta = (ClampMin = "0.0"))
	float PushKnockbackStrength = 900.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Staging|Cinematic", meta = (ClampMin = "0.0"))
	float PushKnockbackUpward = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Staging|Cinematic", meta = (ClampMin = "0.0"))
	float CinematicLightDamage = 9999.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Staging|Cinematic")
	bool bLockPlayerLookAtEnemyDuringGrab = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Staging|Cinematic", meta = (ClampMin = "0.0"))
	float GrabLookLockDuration = 8.f;

	/** 이 거리(cm) 이내로 접근하면 GrabbedPlayer 상태로 전환 → ABP Grap. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Staging|Cinematic", meta = (ClampMin = "50.0"))
	float GrabTriggerDistance = 180.f;

	/** Force Flashlight On 후 Dead 애니 재생 시간 확보용 데미지 지연(초). 0이면 즉시. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Staging|Cinematic", meta = (ClampMin = "0.0"))
	float DeadStateDamageDelay = 1.5f;

	/** ApproachingGrab 구간 이동 속도(cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Staging|Cinematic", meta = (ClampMin = "50.0"))
	float ApproachMoveSpeed = 320.f;

	/** LS 종료 후 재생할 포즈 AnimSequence. 비어 있으면 캐시된 Anim Blueprint를 복구합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Staging|LevelSequence")
	TObjectPtr<UAnimSequence> PostLevelSequenceAnimSequence;

	/** LS 종료 후 SingleNode 대신 ABP Knockdown 상태 머신 사용. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Staging|PostPickupCombat")
	bool bPreferAnimBlueprintKnockdownAfterLevelSequence = true;

	/** LS 종료 후 ABP Knockdown 상태 유지(손전등 획득 전). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Staging|PostPickupCombat")
	bool bHoldKnockdownAfterOpeningCinematic = false;

	/** true면 플레이어 손전등 획득 시 넉다운 해제 + 추격·공격 AI. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Staging|PostPickupCombat")
	bool bActivateCombatOnFlashlightPickup = true;

	/** true면 사망 후 GameInstance에 기록 — 메인메뉴·맵 재입장 시 재스폰하지 않음. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Staging|Persistence")
	bool bPersistDefeatAcrossSessions = true;

	/** 비어 있으면 `맵이름|액터라벨(GetActorNameOrLabel)`로 자동 키 생성. 라벨 없으면 `맵이름|StagingEnemy`. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Staging|Persistence")
	FName StagingDefeatPersistenceKey = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Staging|PostPickupCombat", meta = (ClampMin = "0.0"))
	float PostPickupAttackDamage = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Staging|PostPickupCombat", meta = (ClampMin = "0.0"))
	float PostPickupAggroRadius = 1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Staging|PostPickupCombat", meta = (ClampMin = "50.0"))
	float PostPickupChaseMoveSpeed = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Staging|PostPickupCombat", meta = (ClampMin = "50.0"))
	float PostPickupAttackRange = 180.f;

	/** true면 손전등 획득 시 BasicEnemy 와 동일한 전투 스탯·AI·CC(스턴/둔화) 적용. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Staging|PostPickupCombat")
	bool bUseBasicEnemyCombatAfterPickup = true;

	/** 비어 있으면 현재 Mesh Anim Class 재초기화. BasicEnemy ABP 를 쓰려면 여기에 지정. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Staging|PostPickupCombat")
	TSubclassOf<UAnimInstance> CombatAnimInstanceClassAfterPickup;

	void EnterCinematicMode();
	void ExitCinematicMode();
	void RestoreCombatLocomotionAfterPickup();
	void ApplyBasicEnemyCombatProfile();
	bool IsInPostPickupMeleeRange() const;
	void ApplyKnockdownLaunch(const FVector& PushDir, bool bHorizontalOnly = true);
	void RestoreLevelSequenceAnimation(bool bApplyPostSequencePose);
	void UpdateStagingApproach(float DeltaSeconds);
	virtual void HandleGrabComplete();
	void HandleEnterStandoff();
	void HandleExecuteAutoPush();
	void HandleKnockdownBegin();
	void ApplyDeferredCinematicDeath();

	void StagingDebugLog(const FString& Message, FColor ScreenColor = FColor::Yellow, float ScreenDuration = 2.f) const;

	FName GetStagingDefeatPersistenceKey() const;
	bool IsDefeatPersistedForThisActor() const;
	void MarkDefeatPersistedForThisActor() const;
	void TryMarkDefeatPersisted() const;

	FTimerHandle DeferredDeathTimerHandle;

	UPROPERTY(Transient)
	bool bCinematicModeActive = false;

	float StagingApproachDebugAccumSec = 0.f;

	/** Level Sequence 재생 준비 중. */
	bool bPreparedForLevelSequence = false;
	bool bPostFlashlightPickupCombatActive = false;
	TSubclassOf<UAnimInstance> CachedAnimClassForLevelSequence;
	TSubclassOf<UAnimInstance> DefaultCombatAnimClass;
};
