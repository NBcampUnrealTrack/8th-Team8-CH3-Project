#pragma once

#include "Animation/AnimInstance.h"
#include "AIEnemy/EnemyBase.h"
#include "Cinematic/StagingCinematicTypes.h"
#include "StagingEnemyAnimInstance.generated.h"

/**
 * ABP_StagingEnemy 전용 AnimInstance.
 * Event Graph 캐스트 없이 Staging State / bIsApproachingForGrab 등을 매 틱 동기화합니다.
 */
UCLASS()
class OBLIVIO_API UStagingEnemyAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	/** AnimGraph 전이용 — C++에서 매 틱 동기화. ABP에 같은 이름 로컬 변수 만들지 말 것. */
	UPROPERTY(BlueprintReadOnly, Category = "Staging|Anim", meta = (DisplayName = "Staging State", BlueprintThreadSafe))
	EStagingEnemyCinematicState StagingState = EStagingEnemyCinematicState::Idle;

	UPROPERTY(BlueprintReadOnly, Category = "Staging|Anim", meta = (BlueprintThreadSafe))
	bool bIsApproachingForGrab = false;

	UPROPERTY(BlueprintReadOnly, Category = "Staging|Anim", meta = (BlueprintThreadSafe))
	bool bShouldPlayGrabAnimation = false;

	UPROPERTY(BlueprintReadOnly, Category = "Staging|Anim", meta = (BlueprintThreadSafe))
	bool bShouldPlayKnockdownAnimation = false;

	UPROPERTY(BlueprintReadOnly, Category = "Staging|Anim", meta = (BlueprintThreadSafe))
	bool bShouldPlayMashKnockbackAnimation = false;

	UPROPERTY(BlueprintReadOnly, Category = "Staging|Anim", meta = (BlueprintThreadSafe))
	bool bShouldPlayDeadAnimation = false;

	/** Blend Space / Speed 기반 Walk용 (cm/s). */
	UPROPERTY(BlueprintReadOnly, Category = "Staging|Anim", meta = (BlueprintThreadSafe))
	float GroundSpeed = 0.f;

	/** bIsMoving — Walk 상태 진입 보조. */
	UPROPERTY(BlueprintReadOnly, Category = "Staging|Anim", meta = (BlueprintThreadSafe))
	bool bIsMoving = false;

	/** ABP Knockdown ↔ Idle 전이용. true면 Knockdown 유지. */
	UPROPERTY(BlueprintReadOnly, Category = "Staging|Anim", meta = (BlueprintThreadSafe))
	bool bShouldRemainInKnockdownPose = false;

	/** 손전등 획득 후 전투 AI 활성 여부. */
	UPROPERTY(BlueprintReadOnly, Category = "Staging|Combat", meta = (BlueprintThreadSafe))
	bool bIsPostFlashlightPickupCombatActive = false;

	/**
	 * ABP 전이 규칙용 — Tank ABP 와 동일 변수명.
	 * AnimGraph Variables 에 로컬 "Enemy Crrunt State" 가 있으면 삭제하고 이 멤버만 사용.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Staging|Combat", meta = (DisplayName = "Enemy Crrunt State", BlueprintThreadSafe))
	EEnemyAIState EnemyCruntState = EEnemyAIState::Idle;

	/** StagingEnemyAIState 와 동일(호환용). */
	UPROPERTY(BlueprintReadOnly, Category = "Staging|Combat", meta = (BlueprintThreadSafe))
	EEnemyAIState StagingEnemyAIState = EEnemyAIState::Idle;

	UPROPERTY(BlueprintReadOnly, Category = "Staging|Combat", meta = (BlueprintThreadSafe))
	bool bShouldPlayAttackAnimation = false;

	UPROPERTY(BlueprintReadOnly, Category = "Staging|Combat", meta = (BlueprintThreadSafe))
	bool bShouldPlayChaseAnimation = false;

	/** AnimGraph 검색: "Get Staging State" */
	UFUNCTION(BlueprintPure, Category = "Staging|Anim", meta = (BlueprintThreadSafe))
	EStagingEnemyCinematicState GetStagingState() const { return StagingState; }

	/** AnimGraph 검색: "Is Approaching For Grab" */
	UFUNCTION(BlueprintPure, Category = "Staging|Anim", meta = (BlueprintThreadSafe))
	bool IsApproachingForGrab() const { return bIsApproachingForGrab; }

	/** AnimGraph 검색: "Get Ground Speed" */
	UFUNCTION(BlueprintPure, Category = "Staging|Anim", meta = (BlueprintThreadSafe))
	float GetGroundSpeed() const { return GroundSpeed; }

	/** AnimGraph 검색: "Is Moving" */
	UFUNCTION(BlueprintPure, Category = "Staging|Anim", meta = (BlueprintThreadSafe))
	bool IsMoving() const { return bIsMoving; }

	/** AnimGraph: Knockdown → Idle 전이 조건(!Should Remain In Knockdown Pose). */
	UFUNCTION(BlueprintPure, Category = "Staging|Anim", meta = (BlueprintThreadSafe))
	bool ShouldRemainInKnockdownPose() const { return bShouldRemainInKnockdownPose; }

	UFUNCTION(BlueprintPure, Category = "Staging|Anim", meta = (BlueprintThreadSafe))
	bool ShouldPlayMashKnockbackAnimation() const { return bShouldPlayMashKnockbackAnimation; }

	UFUNCTION(BlueprintPure, Category = "Staging|Combat", meta = (BlueprintThreadSafe))
	EEnemyAIState GetEnemyCruntState() const { return EnemyCruntState; }

	UFUNCTION(BlueprintPure, Category = "Staging|Combat", meta = (BlueprintThreadSafe))
	EEnemyAIState GetStagingEnemyAIState() const { return StagingEnemyAIState; }

	UFUNCTION(BlueprintPure, Category = "Staging|Combat", meta = (BlueprintThreadSafe))
	bool ShouldPlayAttackAnimation() const { return bShouldPlayAttackAnimation; }

	float AnimDebugAccumSec = 0.f;
};
