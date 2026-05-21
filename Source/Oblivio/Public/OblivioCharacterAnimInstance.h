#pragma once

#include "Animation/AnimInstance.h"
#include "Cinematic/StagingCinematicTypes.h"
#include "OblivioCharacterAnimInstance.generated.h"

class AOblivioCharacter;

/**
 * 플레이어 ABP 전용 AnimInstance.
 * 에너미 그랩 시 OblivioCharacter::PlayerCinematicState(Being Grabbed 등)를 매 틱 동기화합니다.
 * ABP에 같은 이름 로컬 변수를 만들지 마세요.
 */
UCLASS()
class OBLIVIO_API UOblivioCharacterAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	/** SetPlayerCinematicState 직후 같은 프레임에 AnimGraph 값을 맞춥니다. */
	void SyncFromCharacter(const AOblivioCharacter* Character);

	UPROPERTY(BlueprintReadOnly, Category = "Cinematic|Anim", meta = (DisplayName = "Player Cinematic State", BlueprintThreadSafe))
	EPlayerCinematicState PlayerCinematicState = EPlayerCinematicState::None;

	UPROPERTY(BlueprintReadOnly, Category = "Cinematic|Anim", meta = (BlueprintThreadSafe))
	bool bInStagingCinematic = false;

	UPROPERTY(BlueprintReadOnly, Category = "Cinematic|Anim", meta = (BlueprintThreadSafe))
	bool bBeingGrabbedAnim = false;

	UPROPERTY(BlueprintReadOnly, Category = "Cinematic|Anim", meta = (BlueprintThreadSafe))
	bool bPushingAnim = false;

	/** AnimGraph Blend Poses by Bool — 연출 SM vs Locomotion 전환. */
	UPROPERTY(BlueprintReadOnly, Category = "Cinematic|Anim", meta = (BlueprintThreadSafe))
	bool bUseCinematicAnimLayer = false;

	UFUNCTION(BlueprintPure, Category = "Cinematic|Anim", meta = (BlueprintThreadSafe))
	EPlayerCinematicState GetPlayerCinematicState() const { return PlayerCinematicState; }

	UFUNCTION(BlueprintPure, Category = "Cinematic|Anim", meta = (BlueprintThreadSafe))
	bool IsBeingGrabbedForAnim() const
	{
		return PlayerCinematicState == EPlayerCinematicState::BeingGrabbed
			|| PlayerCinematicState == EPlayerCinematicState::Standoff;
	}

	UFUNCTION(BlueprintPure, Category = "Cinematic|Anim", meta = (BlueprintThreadSafe))
	bool IsPushingForAnim() const
	{
		return PlayerCinematicState == EPlayerCinematicState::Pushing;
	}

	/** AnimGraph 검색: "Get Use Cinematic Anim Layer" */
	UFUNCTION(BlueprintPure, Category = "Cinematic|Anim", meta = (BlueprintThreadSafe, DisplayName = "Get Use Cinematic Anim Layer"))
	bool GetUseCinematicAnimLayer() const { return bUseCinematicAnimLayer; }

	/** AnimGraph 검색: "Get Being Grabbed Anim" */
	UFUNCTION(BlueprintPure, Category = "Cinematic|Anim", meta = (BlueprintThreadSafe, DisplayName = "Get Being Grabbed Anim"))
	bool GetBeingGrabbedAnim() const { return bBeingGrabbedAnim; }

	/** AnimGraph 검색: "Get Pushing Anim" */
	UFUNCTION(BlueprintPure, Category = "Cinematic|Anim", meta = (BlueprintThreadSafe, DisplayName = "Get Pushing Anim"))
	bool GetPushingAnim() const { return bPushingAnim; }

protected:
	EPlayerCinematicState LastLoggedCinematicState = EPlayerCinematicState::None;
	bool bLastLoggedBeingGrabbed = false;
	bool bLastLoggedPushing = false;
};
