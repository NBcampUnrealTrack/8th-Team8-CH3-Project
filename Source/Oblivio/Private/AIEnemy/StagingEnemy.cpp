#include "AIEnemy/StagingEnemy.h"

#include "Engine/DamageEvents.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "OblivioCharacter.h"
#include "TimerManager.h"

AStagingEnemy::AStagingEnemy()
{
	MaxHealth = 100.f;
	CurrentHealth = MaxHealth;
	AttackDamage = 0.f;
	AggroRadius = 0.f;
	bEnableWandering = false;
}

void AStagingEnemy::BeginPlay()
{
	Super::BeginPlay();

	if (!bAutoStartOpeningCinematic)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		FTimerHandle StartTimer;
		World->GetTimerManager().SetTimer(
			StartTimer,
			[this]()
			{
				if (AOblivioCharacter* Player = Cast<AOblivioCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
				{
					StartOpeningCinematic(Player);
				}
			},
			AutoStartDelaySeconds,
			false);
	}
}

void AStagingEnemy::Tick(float DeltaSeconds)
{
	if (bCinematicModeActive)
	{
		return;
	}

	Super::Tick(DeltaSeconds);
}

EEnemyAIState AStagingEnemy::GetEnemyState() const
{
	if (StagingState == EStagingEnemyCinematicState::Dead)
	{
		return EEnemyAIState::Dead;
	}

	if (bCinematicModeActive)
	{
		return EEnemyAIState::Idle;
	}

	return Super::GetEnemyState();
}

void AStagingEnemy::CommitAttackFromAnimNotify(AActor* OptionalTargetOverride)
{
	(void)OptionalTargetOverride;
	if (!IsAlive() || !bCinematicModeActive)
	{
		return;
	}

	HandleGrabComplete();
}

void AStagingEnemy::DispatchEnemyAttackCommitted(AActor* Target, float DamageAmount,
	TSubclassOf<UDamageType> DamageTypeClass)
{
	(void)Target;
	(void)DamageAmount;
	(void)DamageTypeClass;
}

void AStagingEnemy::EnterCinematicMode()
{
	bCinematicModeActive = true;
	StopEnemyMovement();
	SetEnemyState(EEnemyAIState::Idle, true);
}

void AStagingEnemy::StartOpeningCinematic(AOblivioCharacter* Player)
{
	if (!IsValid(Player) || !IsAlive())
	{
		return;
	}

	LinkedPlayer = Player;
	EnterCinematicMode();
	Player->BeginStagingCinematic(this);
	SetStagingState(EStagingEnemyCinematicState::ApproachingGrab);
}

void AStagingEnemy::SetStagingState(EStagingEnemyCinematicState NewState)
{
	if (StagingState == NewState)
	{
		return;
	}

	const EStagingEnemyCinematicState OldState = StagingState;
	StagingState = NewState;
	OnStagingCinematicStateChanged.Broadcast(this, NewState);

	if (StagingState == EStagingEnemyCinematicState::Dead)
	{
		bCinematicModeActive = false;
	}
}

void AStagingEnemy::HandleStagingCinematicNotify(EStagingEnemyCinematicNotify NotifyEvent)
{
	switch (NotifyEvent)
	{
	case EStagingEnemyCinematicNotify::BeginGrab:
		SetStagingState(EStagingEnemyCinematicState::ApproachingGrab);
		break;
	case EStagingEnemyCinematicNotify::GrabComplete:
		HandleGrabComplete();
		break;
	case EStagingEnemyCinematicNotify::EnterStandoff:
		HandleEnterStandoff();
		break;
	case EStagingEnemyCinematicNotify::ExecuteAutoPush:
		HandleExecuteAutoPush();
		break;
	case EStagingEnemyCinematicNotify::KnockdownBegin:
		HandleKnockdownBegin();
		break;
	case EStagingEnemyCinematicNotify::ForceFlashlightOn:
		ForceFlashlightOnPlayer();
		break;
	case EStagingEnemyCinematicNotify::ApplyLightDamage:
		ApplyCinematicLightDamage();
		break;
	case EStagingEnemyCinematicNotify::CinematicDeath:
		FinishCinematicDeath();
		break;
	default:
		break;
	}

	OnStagingCinematicNotify(NotifyEvent);
}

void AStagingEnemy::HandleGrabComplete()
{
	SetStagingState(EStagingEnemyCinematicState::GrabbedPlayer);

	if (AOblivioCharacter* Player = LinkedPlayer.Get())
	{
		Player->HandlePlayerCinematicNotify(EPlayerCinematicNotify::EnterGrabbed);

		if (bLockPlayerLookAtEnemyDuringGrab)
		{
			Player->ApplyForcedWorldLookTowards(GetActorLocation(), GrabLookLockDuration);
		}
	}
}

void AStagingEnemy::HandleEnterStandoff()
{
	SetStagingState(EStagingEnemyCinematicState::Standoff);

	if (AOblivioCharacter* Player = LinkedPlayer.Get())
	{
		Player->HandlePlayerCinematicNotify(EPlayerCinematicNotify::EnterStandoff);
	}
}

void AStagingEnemy::HandleExecuteAutoPush()
{
	ExecuteAutoPush();
}

void AStagingEnemy::HandleKnockdownBegin()
{
	SetStagingState(EStagingEnemyCinematicState::KnockedDown);
}

void AStagingEnemy::ExecuteAutoPush()
{
	if (!IsAlive()
		|| StagingState == EStagingEnemyCinematicState::PushReaction
		|| StagingState == EStagingEnemyCinematicState::KnockedDown
		|| StagingState == EStagingEnemyCinematicState::FlashlightBurn
		|| StagingState == EStagingEnemyCinematicState::Dead)
	{
		return;
	}

	SetStagingState(EStagingEnemyCinematicState::PushReaction);

	if (AOblivioCharacter* Player = LinkedPlayer.Get())
	{
		Player->HandlePlayerCinematicNotify(EPlayerCinematicNotify::PushSucceeded);

		const FVector PushDir =
			(GetActorLocation() - Player->GetActorLocation()).GetSafeNormal2D();
		LaunchCharacter(PushDir * PushKnockbackStrength + FVector(0.f, 0.f, PushKnockbackUpward), true, true);
	}
}

void AStagingEnemy::ForceFlashlightOnPlayer()
{
	if (AOblivioCharacter* Player = LinkedPlayer.Get())
	{
		Player->ForceFlashlightOnForCinematic();
		Player->HandlePlayerCinematicNotify(EPlayerCinematicNotify::ForceFlashlightOn);
	}

	SetStagingState(EStagingEnemyCinematicState::FlashlightBurn);
}

void AStagingEnemy::ApplyCinematicLightDamage()
{
	if (!IsAlive())
	{
		return;
	}

	FDamageEvent DamageEvent;
	TakeDamage(CinematicLightDamage, DamageEvent, nullptr, LinkedPlayer.Get());
}

void AStagingEnemy::FinishCinematicDeath()
{
	if (!IsAlive())
	{
		SetStagingState(EStagingEnemyCinematicState::Dead);
		return;
	}

	KillEnemy();
}

void AStagingEnemy::Die()
{
	SetStagingState(EStagingEnemyCinematicState::Dead);
	bCinematicModeActive = false;

	if (AOblivioCharacter* Player = LinkedPlayer.Get())
	{
		Player->EndStagingCinematic();
	}

	Super::Die();
}
