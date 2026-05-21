#include "OblivioCharacterAnimInstance.h"

#include "Cinematic/StagingCinematicTypes.h"
#include "OblivioCharacter.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"

#if OBLIVIO_STAGING_GRAB_CINEMATIC_ENABLED
DEFINE_LOG_CATEGORY_STATIC(LogPlayerCinematicAnim, Log, All);
#endif

void UOblivioCharacterAnimInstance::SyncFromCharacter(const AOblivioCharacter* Character)
{
	PlayerCinematicState = EPlayerCinematicState::None;
	bInStagingCinematic = false;
	bBeingGrabbedAnim = false;
	bPushingAnim = false;
	bUseCinematicAnimLayer = false;

#if !OBLIVIO_STAGING_GRAB_CINEMATIC_ENABLED
	(void)Character;
	return;
#else
	if (!IsValid(Character))
	{
		return;
	}

	PlayerCinematicState = Character->GetPlayerCinematicState();
	bInStagingCinematic = Character->IsInStagingCinematic();
	bBeingGrabbedAnim = Character->ShouldPlayBeingGrabbedAnimation();
	bPushingAnim = Character->ShouldPlayPushingAnimation();
	bUseCinematicAnimLayer = bBeingGrabbedAnim || bPushingAnim;
#endif
}

void UOblivioCharacterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

#if !OBLIVIO_STAGING_GRAB_CINEMATIC_ENABLED
	return;
#else
	const AOblivioCharacter* Character = Cast<AOblivioCharacter>(TryGetPawnOwner());
	SyncFromCharacter(Character);

	if (!IsValid(Character))
	{
		return;
	}

	const bool bDebug = Character->IsPlayerCinematicAnimDebugEnabled();
	if (!bDebug)
	{
		return;
	}

	const bool bStateChanged = PlayerCinematicState != LastLoggedCinematicState
		|| bBeingGrabbedAnim != bLastLoggedBeingGrabbed
		|| bPushingAnim != bLastLoggedPushing;

	if (bStateChanged)
	{
		LastLoggedCinematicState = PlayerCinematicState;
		bLastLoggedBeingGrabbed = bBeingGrabbedAnim;
		bLastLoggedPushing = bPushingAnim;

		const FString Msg = FString::Printf(
			TEXT("ABP: State=%s InStaging=%d Grab=%d Push=%d UseLayer=%d"),
			*UEnum::GetDisplayValueAsText(PlayerCinematicState).ToString(),
			bInStagingCinematic ? 1 : 0,
			bBeingGrabbedAnim ? 1 : 0,
			bPushingAnim ? 1 : 0,
			bUseCinematicAnimLayer ? 1 : 0);

		UE_LOG(LogPlayerCinematicAnim, Log, TEXT("[PlayerABP] %s: %s"), *GetNameSafe(Character), *Msg);

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				2.f,
				FColor::Cyan,
				FString::Printf(TEXT("[PlayerABP] %s"), *Msg));
		}
	}
#endif
}
