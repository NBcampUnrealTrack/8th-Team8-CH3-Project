#pragma once

#include "Animation/AnimInstance.h"
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

	UFUNCTION(BlueprintPure, Category = "Staging|Anim", meta = (BlueprintThreadSafe))
	bool ShouldPlayMashKnockbackAnimation() const { return bShouldPlayMashKnockbackAnimation; }

	float AnimDebugAccumSec = 0.f;
};
