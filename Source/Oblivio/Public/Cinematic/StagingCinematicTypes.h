#pragma once

#include "CoreMinimal.h"
#include "StagingCinematicTypes.generated.h"

/** 시작 방 그랩 연출(C++). 0 = 비활성 — Level Sequence 등 외부 연출 사용 시. */
#ifndef OBLIVIO_STAGING_GRAB_CINEMATIC_ENABLED
#define OBLIVIO_STAGING_GRAB_CINEMATIC_ENABLED 0
#endif

/** 연출용 에너미 FSM. ABP·블루프린트에서 StagingState 로 읽습니다. */
UENUM(BlueprintType)
enum class EStagingEnemyCinematicState : uint8
{
	Idle UMETA(DisplayName = "Idle"),
	ApproachingGrab UMETA(DisplayName = "Approaching Grab"),
	GrabbedPlayer UMETA(DisplayName = "Grabbed Player"),
	Standoff UMETA(DisplayName = "Standoff"),
	PushReaction UMETA(DisplayName = "Push Reaction"),
	KnockedDown UMETA(DisplayName = "Knocked Down"),
	FlashlightBurn UMETA(DisplayName = "Flashlight Burn"),
	Dead UMETA(DisplayName = "Dead"),
};

/** 플레이어 연출 FSM — ABP_OblivioCharacter AnimGraph 전환에 사용. */
UENUM(BlueprintType)
enum class EPlayerCinematicState : uint8
{
	None UMETA(DisplayName = "None"),
	BeingGrabbed UMETA(DisplayName = "Being Grabbed (붙잡힘)"),
	Standoff UMETA(DisplayName = "Standoff"),
	Pushing UMETA(DisplayName = "Pushing"),
	Released UMETA(DisplayName = "Released"),
};

/** 연출 에너미 AnimNotify 이벤트 종류. 몽타주·시퀀스에 배치합니다. */
UENUM(BlueprintType)
enum class EStagingEnemyCinematicNotify : uint8
{
	None UMETA(DisplayName = "None"),
	BeginGrab UMETA(DisplayName = "Begin Grab"),
	GrabComplete UMETA(DisplayName = "Grab Complete"),
	EnterStandoff UMETA(DisplayName = "Enter Standoff"),
	ExecuteAutoPush UMETA(DisplayName = "Execute Auto Push"),
	KnockdownBegin UMETA(DisplayName = "Knockdown Begin"),
	ForceFlashlightOn UMETA(DisplayName = "Force Flashlight On"),
	ApplyLightDamage UMETA(DisplayName = "Apply Light Damage"),
	CinematicDeath UMETA(DisplayName = "Cinematic Death"),
};

/** 플레이어 AnimNotify 이벤트 종류. */
UENUM(BlueprintType)
enum class EPlayerCinematicNotify : uint8
{
	None UMETA(DisplayName = "None"),
	EnterGrabbed UMETA(DisplayName = "Enter Grabbed"),
	EnterStandoff UMETA(DisplayName = "Enter Standoff"),
	ExecuteAutoPush UMETA(DisplayName = "Execute Auto Push"),
	PushSucceeded UMETA(DisplayName = "Push Succeeded"),
	ReleaseFromGrab UMETA(DisplayName = "Release From Grab"),
	ForceFlashlightOn UMETA(DisplayName = "Force Flashlight On"),
	RestoreControl UMETA(DisplayName = "Restore Control"),
};
