#include "AIEnemy/StagingEnemy.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "OblivioCharacter.h"
#include "OblivioGameInstance.h"
#include "TimerManager.h"
#include "Math/UnrealMathUtility.h"

namespace
{
	static FName GetLevelNameFromWorld(const UWorld* World)
	{
		return World ? FName(*UGameplayStatics::GetCurrentLevelName(World, true)) : NAME_None;
	}

	static bool IsStagingEnemyActorDefeated(const UOblivioGameInstance* GI, const UWorld* World, const AActor* Actor)
	{
		if (!GI || !World || !IsValid(Actor))
		{
			return false;
		}

		const FName LevelName = GetLevelNameFromWorld(World);
		if (!LevelName.IsNone() && GI->IsStagingEnemyDefeatedForLevel(LevelName))
		{
			return true;
		}

		if (!LevelName.IsNone())
		{
			const FString LevelPrefix = LevelName.ToString() + TEXT("|");
			for (const FName Key : GI->DefeatedStagingEnemyKeys)
			{
				const FString KeyString = Key.ToString();
				if (KeyString.StartsWith(LevelPrefix) || Key == LevelName)
				{
					return true;
				}
			}
		}

		return false;
	}

	static bool IsStagingEnemyWorldActor(const AActor* Actor)
	{
		if (!IsValid(Actor))
		{
			return false;
		}

		if (Actor->IsA(AStagingEnemy::StaticClass()))
		{
			return true;
		}

		if (Actor->ActorHasTag(FName(TEXT("StagingEnemy"))))
		{
			return true;
		}

		const FString ActorName = Actor->GetName();
		return ActorName.Contains(TEXT("StagingEnemy"), ESearchCase::IgnoreCase);
	}
}

AStagingEnemy::AStagingEnemy()
{
	Tags.Add(FName(TEXT("StagingEnemy")));

	MaxHealth = 100.f;
	CurrentHealth = MaxHealth;
	AttackDamage = 0.f;
	AggroRadius = 0.f;
	bEnableWandering = false;
	bEnableLightTracking = false;
}

void AStagingEnemy::BeginPlay()
{
	if (bPersistDefeatAcrossSessions)
	{
		if (UOblivioGameInstance* GI = Cast<UOblivioGameInstance>(GetGameInstance()))
		{
			GI->LoadSessionPersistence();
		}
	}

	if (bPersistDefeatAcrossSessions && IsDefeatPersistedForThisActor())
	{
		Destroy();
		return;
	}

	Super::BeginPlay();

	if (USkeletalMeshComponent* SkelMesh = GetMesh())
	{
		if (TSubclassOf<UAnimInstance> AnimClass = SkelMesh->GetAnimClass())
		{
			DefaultCombatAnimClass = AnimClass;
		}
	}

	if (bPersistDefeatAcrossSessions)
	{
		if (UWorld* World = GetWorld())
		{
			TWeakObjectPtr<AStagingEnemy> WeakThis(this);
			World->GetTimerManager().SetTimerForNextTick([WeakThis]()
			{
				if (WeakThis.IsValid()
					&& WeakThis->bPersistDefeatAcrossSessions
					&& WeakThis->IsDefeatPersistedForThisActor())
				{
					WeakThis->Destroy();
				}
			});
		}
	}

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

void AStagingEnemy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bPersistDefeatAcrossSessions
		&& (StagingState == EStagingEnemyCinematicState::Dead || !IsAlive()))
	{
		TryMarkDefeatPersisted();
	}

	Super::EndPlay(EndPlayReason);
}

void AStagingEnemy::Tick(float DeltaSeconds)
{
	if (!bPostFlashlightPickupCombatActive && bCinematicModeActive)
	{
		return;
	}

	Super::Tick(DeltaSeconds);
}

EEnemyAIState AStagingEnemy::GetEnemyState() const
{
	if (bPostFlashlightPickupCombatActive)
	{
		if (StagingState == EStagingEnemyCinematicState::Dead)
		{
			return EEnemyAIState::Dead;
		}

		// Stunned 래퍼 없이 Chase/Attack FSM 그대로 ABP·Tick 에 전달.
		return EnemyState;
	}

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

void AStagingEnemy::ApplyCCStun(float /*Duration*/)
{
	if (bPostFlashlightPickupCombatActive)
	{
		// 전투 구간: CC 스턴으로 UpdateAttack 이 막히지 않게(손전등·CombatComp 포함).
		return;
	}
}

void AStagingEnemy::ApplyCCSlow(float /*SpeedMultiplier*/, float /*Duration*/)
{
	if (bPostFlashlightPickupCombatActive)
	{
		return;
	}
}

void AStagingEnemy::OnLightHit(float Intensity, float Duration)
{
	if (bPostFlashlightPickupCombatActive)
	{
		if (!IsAlive())
		{
			return;
		}

		const float ClampedDuration = FMath::Max(0.f, Duration);
		const float ClampedIntensity = FMath::Clamp(Intensity, 0.f, 1.f);
		OnEnemyLightHit.Broadcast(this, ClampedIntensity, ClampedDuration);
		// 손전등 피격 시 Stunned FSM 으로 덮지 않음 — 근접 Attack 전환은 UpdateState/SelectStateWhileAggroed 가 담당.
		return;
	}

	if (!IsAlive())
	{
		return;
	}

	const float ClampedDuration = FMath::Max(0.f, Duration);
	const float ClampedIntensity = FMath::Clamp(Intensity, 0.f, 1.f);
	OnEnemyLightHit.Broadcast(this, ClampedIntensity, ClampedDuration);
}

bool AStagingEnemy::IsInPostPickupMeleeRange() const
{
	if (!IsValid(TargetActor))
	{
		return false;
	}

	const float EffectiveRange = FMath::Max(AttackRange, 120.f);
	const float RangeSq = FMath::Square(EffectiveRange);
	const FVector A = GetActorLocation();
	const FVector B = TargetActor->GetActorLocation();

	return FVector::DistSquared(A, B) <= RangeSq
		|| FVector::DistSquared(FVector(A.X, A.Y, 0.f), FVector(B.X, B.Y, 0.f)) <= RangeSq;
}

bool AStagingEnemy::IsTargetInAttackRange() const
{
	if (!bPostFlashlightPickupCombatActive)
	{
		return false;
	}

	return IsInPostPickupMeleeRange();
}

EEnemyAIState AStagingEnemy::SelectStateWhileAggroed() const
{
	if (!bPostFlashlightPickupCombatActive)
	{
		return Super::SelectStateWhileAggroed();
	}

	// 베이스는 forced Chase를 스윙 락보다 먼저 검사 — ABP Attack 전이가 Chase로 덮임.
	if (IsMeleeAttackSwingStateLocked())
	{
		return EEnemyAIState::Attack;
	}
	if (IsMeleePostAttackForcedChaseActive())
	{
		return EEnemyAIState::Chase;
	}

	return IsTargetInAttackRange() ? EEnemyAIState::Attack : EEnemyAIState::Chase;
}

void AStagingEnemy::UpdateState()
{
	if (!bPostFlashlightPickupCombatActive)
	{
		Super::UpdateState();
		return;
	}

	if (!IsAlive())
	{
		SetEnemyState(EEnemyAIState::Dead);
		return;
	}

	if (!IsValid(TargetActor))
	{
		FindDefaultTarget();
	}

	if (!IsValid(TargetActor))
	{
		SetEnemyState(EEnemyAIState::Idle);
		return;
	}

	// TrackLight·Investigate 등으로 Chase/Attack FSM 이 가려지지 않게 — BasicEnemy 어그로 전투만.
	ApplyAggroCombatTransientCleanup();
	SetEnemyState(SelectStateWhileAggroed());
}

void AStagingEnemy::CommitAttackFromAnimNotify(AActor* OptionalTargetOverride)
{
	if (bPostFlashlightPickupCombatActive)
	{
		Super::CommitAttackFromAnimNotify(OptionalTargetOverride);
		return;
	}

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
	if (bPostFlashlightPickupCombatActive)
	{
		Super::DispatchEnemyAttackCommitted(Target, DamageAmount, DamageTypeClass);
		return;
	}

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

void AStagingEnemy::ExitCinematicMode()
{
	bCinematicModeActive = false;

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->SetMovementMode(MOVE_Walking);
	}
}

void AStagingEnemy::ApplyKnockdownLaunch(const FVector& PushDir, bool bHorizontalOnly)
{
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->SetMovementMode(MOVE_Walking);
		MoveComp->StopMovementImmediately();
	}

	FVector LaunchVelocity = PushDir.GetSafeNormal2D() * PushKnockbackStrength;
	if (!bHorizontalOnly)
	{
		LaunchVelocity.Z = PushKnockbackUpward;
	}

	LaunchCharacter(LaunchVelocity, true, true);
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
		TryMarkDefeatPersisted();
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
		ApplyKnockdownLaunch(PushDir, true);
	}
}

void AStagingEnemy::HandlePlayerExecuteAutoPush()
{
	ExecuteAutoPush();
}

void AStagingEnemy::HandlePlayerForceFlashlightOn()
{
	SetStagingState(EStagingEnemyCinematicState::Dead);
	ApplyCinematicLightDamage();
	if (IsAlive())
	{
		FinishCinematicDeath();
	}
}

bool AStagingEnemy::IsApproachingForGrab() const
{
	return StagingState == EStagingEnemyCinematicState::ApproachingGrab && IsAlive();
}

bool AStagingEnemy::ShouldPlayGrabAnimation() const
{
	return StagingState == EStagingEnemyCinematicState::GrabbedPlayer
		|| StagingState == EStagingEnemyCinematicState::Standoff;
}

bool AStagingEnemy::ShouldPlayKnockdownAnimation() const
{
	if (bPostFlashlightPickupCombatActive)
	{
		return false;
	}

	if (bHoldKnockdownAfterOpeningCinematic)
	{
		return true;
	}

	return StagingState == EStagingEnemyCinematicState::KnockedDown
		|| StagingState == EStagingEnemyCinematicState::PushReaction;
}

bool AStagingEnemy::ShouldRemainInKnockdownPose() const
{
	return bHoldKnockdownAfterOpeningCinematic && !bPostFlashlightPickupCombatActive;
}

bool AStagingEnemy::ShouldPlayDeadAnimation() const
{
	return StagingState == EStagingEnemyCinematicState::Dead
		|| StagingState == EStagingEnemyCinematicState::FlashlightBurn;
}

float AStagingEnemy::GetDistanceToLinkedPlayer() const
{
	const AOblivioCharacter* Player = LinkedPlayer.Get();
	if (!IsValid(Player))
	{
		return TNumericLimits<float>::Max();
	}

	return FVector::Dist(GetActorLocation(), Player->GetActorLocation());
}

void AStagingEnemy::ForceFlashlightOnPlayer()
{
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
		TryMarkDefeatPersisted();
		SetStagingState(EStagingEnemyCinematicState::Dead);
		return;
	}

	KillEnemy();
}

void AStagingEnemy::Die()
{
	TryMarkDefeatPersisted();

	SetStagingState(EStagingEnemyCinematicState::Dead);
	bCinematicModeActive = false;

	if (AOblivioCharacter* Player = LinkedPlayer.Get())
	{
		Player->EndStagingCinematic();
	}

	Super::Die();
}

void AStagingEnemy::PrepareForLevelSequencePlayback()
{
	if (bPreparedForLevelSequence)
	{
		return;
	}

	USkeletalMeshComponent* SkelMesh = GetMesh();
	if (!SkelMesh)
	{
		return;
	}

	TSubclassOf<UAnimInstance> AnimClassToCache = SkelMesh->GetAnimClass();
	if (!AnimClassToCache)
	{
		AnimClassToCache = SkelMesh->AnimClass;
	}

	if (AnimClassToCache)
	{
		CachedAnimClassForLevelSequence = AnimClassToCache;
		SkelMesh->SetAnimInstanceClass(nullptr);
	}

	bPreparedForLevelSequence = true;
	bCinematicModeActive = false;

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
		MoveComp->DisableMovement();
	}
}

void AStagingEnemy::RestoreLevelSequenceAnimation(bool bApplyPostSequencePose)
{
	USkeletalMeshComponent* SkelMesh = GetMesh();
	if (!SkelMesh)
	{
		bPreparedForLevelSequence = false;
		CachedAnimClassForLevelSequence = nullptr;
		return;
	}

	SkelMesh->bPauseAnims = false;

	if (bApplyPostSequencePose && IsValid(PostLevelSequenceAnimSequence))
	{
		SkelMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		SkelMesh->PlayAnimation(PostLevelSequenceAnimSequence, false);
	}
	else if (TSubclassOf<UAnimInstance> ClassToRestore = CachedAnimClassForLevelSequence)
	{
		SkelMesh->SetAnimInstanceClass(ClassToRestore);
		SkelMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		SkelMesh->InitAnim(true);
	}

	bPreparedForLevelSequence = false;
	CachedAnimClassForLevelSequence = nullptr;

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->SetMovementMode(MOVE_Walking);
	}
}

void AStagingEnemy::RestoreAfterLevelSequenceAbort()
{
	if (!bPreparedForLevelSequence)
	{
		return;
	}

	RestoreLevelSequenceAnimation(false);
}

void AStagingEnemy::RestoreAfterLevelSequenceFinished()
{
	if (!bPreparedForLevelSequence)
	{
		return;
	}

	bCinematicModeActive = true;
	const bool bUseAbpKnockdown = bPreferAnimBlueprintKnockdownAfterLevelSequence;
	RestoreLevelSequenceAnimation(!bUseAbpKnockdown);

	SetStagingState(EStagingEnemyCinematicState::KnockedDown);
	bHoldKnockdownAfterOpeningCinematic = true;
}

void AStagingEnemy::PrepareAllForLevelSequence(const UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		return;
	}

	TArray<AActor*> StagingEnemies;
	UGameplayStatics::GetAllActorsOfClass(World, AStagingEnemy::StaticClass(), StagingEnemies);
	for (AActor* Actor : StagingEnemies)
	{
		if (AStagingEnemy* StagingEnemy = Cast<AStagingEnemy>(Actor))
		{
			StagingEnemy->PrepareForLevelSequencePlayback();
		}
	}
}

void AStagingEnemy::RestoreAllAfterLevelSequenceAbort(const UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		return;
	}

	TArray<AActor*> StagingEnemies;
	UGameplayStatics::GetAllActorsOfClass(World, AStagingEnemy::StaticClass(), StagingEnemies);
	for (AActor* Actor : StagingEnemies)
	{
		if (AStagingEnemy* StagingEnemy = Cast<AStagingEnemy>(Actor))
		{
			StagingEnemy->RestoreAfterLevelSequenceAbort();
		}
	}
}

void AStagingEnemy::RestoreAllAfterLevelSequenceFinished(const UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		return;
	}

	TArray<AActor*> StagingEnemies;
	UGameplayStatics::GetAllActorsOfClass(World, AStagingEnemy::StaticClass(), StagingEnemies);
	for (AActor* Actor : StagingEnemies)
	{
		if (AStagingEnemy* StagingEnemy = Cast<AStagingEnemy>(Actor))
		{
			StagingEnemy->RestoreAfterLevelSequenceFinished();
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[LevelSequence] Restored animation for %d StagingEnemy actor(s)"), StagingEnemies.Num());
}

void AStagingEnemy::RestoreCombatLocomotionAfterPickup()
{
	USkeletalMeshComponent* SkelMesh = GetMesh();
	if (!SkelMesh)
	{
		return;
	}

	SkelMesh->bPauseAnims = false;
	SkelMesh->Stop();

	TSubclassOf<UAnimInstance> ClassToRestore = CombatAnimInstanceClassAfterPickup;
	if (!ClassToRestore)
	{
		ClassToRestore = CachedAnimClassForLevelSequence;
	}
	if (!ClassToRestore)
	{
		ClassToRestore = DefaultCombatAnimClass;
	}

	if (ClassToRestore)
	{
		SkelMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		if (SkelMesh->GetAnimClass() != ClassToRestore)
		{
			SkelMesh->SetAnimInstanceClass(ClassToRestore);
		}
		SkelMesh->InitAnim(true);
	}

	bPreparedForLevelSequence = false;
}

void AStagingEnemy::ApplyBasicEnemyCombatProfile()
{
	// ABasicEnemy Class Defaults + PostPickup 오버라이드. BP 에 0 이 저장돼 있으면 최소값 적용.
	AttackDamage = FMath::Max(PostPickupAttackDamage, 1.f);
	AggroRadius = PostPickupAggroRadius > KINDA_SMALL_NUMBER ? PostPickupAggroRadius : 1000.f;
	AttackRange = FMath::Max(PostPickupAttackRange, 120.f);
	AttackCooldown = 1.f;
	ChaseAcceptanceRadius = 55.f;
	ChaseProximityBuffer = 48.f;
	ChaseMoveSpeed = PostPickupChaseMoveSpeed;
	MoveSpeed = 350.f;
	MeleePostAttackForcedChaseDurationSeconds = 0.f;
	bEnableWandering = false;
	bEnableLightTracking = false;
}

void AStagingEnemy::ActivatePostFlashlightPickupCombat(AOblivioCharacter* Player)
{
	if (!bActivateCombatOnFlashlightPickup || bPostFlashlightPickupCombatActive || !IsAlive())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DeferredDeathTimerHandle);
	}

	bPostFlashlightPickupCombatActive = true;
	bHoldKnockdownAfterOpeningCinematic = false;
	bCinematicModeActive = false;
	LinkedPlayer = Player;

	ClearCCStun();
	ClearCCSlow();
	ClearMeleeAttackSwingStateLock();

	SetStagingState(EStagingEnemyCinematicState::Idle);

	RestoreCombatLocomotionAfterPickup();
	ExitCinematicMode();

	if (bUseBasicEnemyCombatAfterPickup)
	{
		ApplyBasicEnemyCombatProfile();
	}
	else
	{
		AttackDamage = FMath::Max(PostPickupAttackDamage, 1.f);
		AggroRadius = PostPickupAggroRadius;
		ChaseMoveSpeed = PostPickupChaseMoveSpeed;
		MoveSpeed = PostPickupChaseMoveSpeed;
		AttackRange = PostPickupAttackRange;
		bEnableWandering = false;
		bEnableLightTracking = false;
	}

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->SetMovementMode(MOVE_Walking);
	}

	RefreshWalkSpeedFromSources();

	if (IsValid(Player))
	{
		SetTargetActor(Player);
		Player->EndStagingCinematic();
	}
	else if (AOblivioCharacter* FallbackPlayer = Cast<AOblivioCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		SetTargetActor(FallbackPlayer);
	}

	SetEnemyState(EEnemyAIState::Chase, true);
	UpdateState();

	const float MaxChaseAcceptance = FMath::Max(1.f, AttackRange - ChaseProximityBuffer);
	const float EffectiveChaseAcceptance = FMath::Min(ChaseAcceptanceRadius, MaxChaseAcceptance);
	StagingDebugLog(
		FString::Printf(
			TEXT("Combat: AttackRange=%.0f ChaseAccept=%.0f (eff %.0f) MeleeNotifySlack=%.0f"),
			AttackRange,
			ChaseAcceptanceRadius,
			EffectiveChaseAcceptance,
			MeleeCommitRangeSlackCm),
		FColor::Orange,
		4.f);

	if (bDebugStagingCinematic)
	{
		bShowMeleeAttackRangeIndicator = true;
		UpdateMeleeAttackRangeIndicatorVisual();
	}
}

void AStagingEnemy::ActivateAllAfterFlashlightPickup(const UObject* WorldContextObject, AOblivioCharacter* Player)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		return;
	}

	TArray<AActor*> StagingEnemies;
	UGameplayStatics::GetAllActorsOfClass(World, AStagingEnemy::StaticClass(), StagingEnemies);
	for (AActor* Actor : StagingEnemies)
	{
		if (AStagingEnemy* StagingEnemy = Cast<AStagingEnemy>(Actor))
		{
			StagingEnemy->ActivatePostFlashlightPickupCombat(Player);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[StagingEnemy] Activated post-flashlight combat on %d actor(s)"), StagingEnemies.Num());
}

void AStagingEnemy::DestroyAllDefeatedInWorld(const UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		return;
	}

	const UOblivioGameInstance* GI = Cast<UOblivioGameInstance>(UGameplayStatics::GetGameInstance(WorldContextObject));
	if (!GI)
	{
		return;
	}

	if (UOblivioGameInstance* MutableGI = const_cast<UOblivioGameInstance*>(GI))
	{
		MutableGI->LoadSessionPersistence();
	}

	TSet<AActor*> CandidateActors;

	for (TActorIterator<AStagingEnemy> It(World); It; ++It)
	{
		CandidateActors.Add(*It);
	}

	TArray<AActor*> TaggedActors;
	UGameplayStatics::GetAllActorsWithTag(World, FName(TEXT("StagingEnemy")), TaggedActors);
	for (AActor* Actor : TaggedActors)
	{
		if (IsValid(Actor))
		{
			CandidateActors.Add(Actor);
		}
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (IsStagingEnemyWorldActor(*It))
		{
			CandidateActors.Add(*It);
		}
	}

	for (AActor* Actor : CandidateActors)
	{
		if (AStagingEnemy* StagingEnemy = Cast<AStagingEnemy>(Actor))
		{
			if (StagingEnemy->bPersistDefeatAcrossSessions && StagingEnemy->IsDefeatPersistedForThisActor())
			{
				StagingEnemy->Destroy();
			}
		}
		else if (IsStagingEnemyActorDefeated(GI, World, Actor))
		{
			Actor->Destroy();
		}
	}
}

FName AStagingEnemy::GetStagingDefeatPersistenceKey() const
{
	if (!StagingDefeatPersistenceKey.IsNone())
	{
		return StagingDefeatPersistenceKey;
	}

	const UWorld* World = GetWorld();
	const FName LevelName = GetLevelNameFromWorld(World);
	if (LevelName.IsNone())
	{
		return NAME_None;
	}

	const FString StableActorName = GetActorNameOrLabel();
	if (StableActorName.IsEmpty())
	{
		return FName(*FString::Printf(TEXT("%s|StagingEnemy"), *LevelName.ToString()));
	}

	return FName(*FString::Printf(TEXT("%s|%s"), *LevelName.ToString(), *StableActorName));
}

bool AStagingEnemy::IsDefeatPersistedForThisActor() const
{
	const FName Key = GetStagingDefeatPersistenceKey();
	if (Key.IsNone())
	{
		return false;
	}

	const UOblivioGameInstance* GI = Cast<UOblivioGameInstance>(GetGameInstance());
	if (!GI)
	{
		return false;
	}

	if (GI->IsStagingEnemyDefeated(Key))
	{
		return true;
	}

	const UWorld* World = GetWorld();
	const FName LevelName = GetLevelNameFromWorld(World);
	if (LevelName.IsNone())
	{
		return false;
	}

	if (GI->IsStagingEnemyDefeatedForLevel(LevelName))
	{
		return true;
	}

	const FName LevelOnlyKey = FName(*FString::Printf(TEXT("%s|StagingEnemy"), *LevelName.ToString()));
	return GI->IsStagingEnemyDefeated(LevelOnlyKey);
}

void AStagingEnemy::TryMarkDefeatPersisted() const
{
	if (bPersistDefeatAcrossSessions)
	{
		MarkDefeatPersistedForThisActor();
	}
}

void AStagingEnemy::MarkDefeatPersistedForThisActor() const
{
	const FName Key = GetStagingDefeatPersistenceKey();
	if (Key.IsNone())
	{
		return;
	}

	if (UOblivioGameInstance* GI = Cast<UOblivioGameInstance>(GetGameInstance()))
	{
		GI->MarkStagingEnemyDefeated(Key);

		const UWorld* World = GetWorld();
		const FName LevelName = GetLevelNameFromWorld(World);
		if (!LevelName.IsNone())
		{
			GI->MarkStagingEnemyDefeatedForLevel(LevelName);
		}

		GI->SaveSessionPersistence();
	}
}

void AStagingEnemy::StagingDebugLog(const FString& Message, FColor ScreenColor, float ScreenDuration) const
{
	if (!bDebugStagingCinematic)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[StagingEnemy] %s: %s"), *GetName(), *Message);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			ScreenDuration,
			ScreenColor,
			FString::Printf(TEXT("[StagingEnemy] %s"), *Message));
	}
}
