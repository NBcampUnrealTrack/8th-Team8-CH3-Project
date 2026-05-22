#include "AIEnemy/StagingEnemyAnimInstance.h"

#include "AIEnemy/StagingEnemy.h"
#include "Cinematic/StagingCinematicTypes.h"
#include "Engine/Engine.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"

void UStagingEnemyAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

#if OBLIVIO_STAGING_GRAB_CINEMATIC_ENABLED
	const EStagingEnemyCinematicState PrevAnimState = StagingState;
	const bool bPrevApproaching = bIsApproachingForGrab;
	const bool bPrevGrab = bShouldPlayGrabAnimation;
	const bool bPrevKnock = bShouldPlayKnockdownAnimation;
	const bool bPrevMashKnockback = bShouldPlayMashKnockbackAnimation;
	const bool bPrevDead = bShouldPlayDeadAnimation;
	const bool bPrevMoving = bIsMoving;
#endif

	StagingState = EStagingEnemyCinematicState::Idle;
	bIsApproachingForGrab = false;
	bShouldPlayGrabAnimation = false;
	bShouldPlayKnockdownAnimation = false;
	bShouldPlayMashKnockbackAnimation = false;
	bShouldPlayDeadAnimation = false;
	GroundSpeed = 0.f;
	bIsMoving = false;

	APawn* const OwnerPawn = TryGetPawnOwner();
	const AStagingEnemy* const StagingEnemy = OwnerPawn ? Cast<AStagingEnemy>(OwnerPawn) : nullptr;
	if (!IsValid(StagingEnemy) || !StagingEnemy->IsAlive())
	{
		return;
	}

	StagingState = StagingEnemy->GetStagingState();
	bIsApproachingForGrab = StagingEnemy->IsApproachingForGrab();
	bShouldPlayGrabAnimation = StagingEnemy->ShouldPlayGrabAnimation();
	bShouldPlayKnockdownAnimation = StagingEnemy->ShouldPlayKnockdownAnimation();
	bShouldPlayMashKnockbackAnimation = StagingEnemy->ShouldPlayMashKnockbackAnimation();
	bShouldPlayDeadAnimation = StagingEnemy->ShouldPlayDeadAnimation();

	if (const ACharacter* const Character = Cast<ACharacter>(OwnerPawn))
	{
		if (const UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
		{
			GroundSpeed = MoveComp->Velocity.Size2D();
		}
	}

	const bool bSuppressLocomotionForCinematic =
		StagingState == EStagingEnemyCinematicState::PushReaction
		|| StagingState == EStagingEnemyCinematicState::MashEscapeSuccess
		|| StagingState == EStagingEnemyCinematicState::KnockedDown
		|| bShouldPlayDeadAnimation;

	if (bSuppressLocomotionForCinematic)
	{
		GroundSpeed = 0.f;
		bIsMoving = false;
	}
	else
	{
		bIsMoving = GroundSpeed > 10.f || bIsApproachingForGrab;
	}

#if OBLIVIO_STAGING_GRAB_CINEMATIC_ENABLED
	if (!StagingEnemy->IsStagingDebugEnabled())
	{
		return;
	}

	const bool bStateChanged = StagingState != PrevAnimState
		|| bIsApproachingForGrab != bPrevApproaching
		|| bShouldPlayGrabAnimation != bPrevGrab
		|| bShouldPlayKnockdownAnimation != bPrevKnock
		|| bShouldPlayMashKnockbackAnimation != bPrevMashKnockback
		|| bShouldPlayDeadAnimation != bPrevDead
		|| bIsMoving != bPrevMoving;

	if (bStateChanged)
	{
		const FString Msg = FString::Printf(
			TEXT("ABP: AnimState=%s Approaching=%d Grab=%d Knock=%d Dead=%d Speed=%.0f Moving=%d"),
			*UEnum::GetDisplayValueAsText(StagingState).ToString(),
			bIsApproachingForGrab ? 1 : 0,
			bShouldPlayGrabAnimation ? 1 : 0,
			bShouldPlayKnockdownAnimation ? 1 : 0,
			bShouldPlayDeadAnimation ? 1 : 0,
			GroundSpeed,
			bIsMoving ? 1 : 0);

		UE_LOG(LogTemp, Log, TEXT("[StagingABP] %s: %s"), *GetNameSafe(OwnerPawn), *Msg);

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				2.f,
				FColor::Silver,
				FString::Printf(TEXT("[StagingABP] %s"), *Msg));
		}
	}
	else if (bIsApproachingForGrab)
	{
		AnimDebugAccumSec += DeltaSeconds;
		if (AnimDebugAccumSec >= 0.5f)
		{
			AnimDebugAccumSec = 0.f;
			const FString Msg = FString::Printf(TEXT("ABP Walk: Speed=%.0f Moving=%d"), GroundSpeed, bIsMoving ? 1 : 0);
			UE_LOG(LogTemp, Log, TEXT("[StagingABP] %s: %s"), *GetNameSafe(OwnerPawn), *Msg);
		}
	}
#endif
}
