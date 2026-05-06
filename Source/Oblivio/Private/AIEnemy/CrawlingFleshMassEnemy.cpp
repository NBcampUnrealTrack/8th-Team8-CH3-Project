#include "AIEnemy/CrawlingFleshMassEnemy.h"
#include "AIEnemy/EnemySpawner.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"
#include "AIController.h"
#include "Engine/World.h"

ACrawlingFleshMassEnemy::ACrawlingFleshMassEnemy()
{
	PrimaryActorTick.bCanEverTick = true;

	MaxHealth = 22.f;
	CurrentHealth = MaxHealth;
	MoveSpeed = 240.f;
	ChaseMoveSpeed = 460.f;
	AttackDamage = 6.f;
	AttackRange = 130.f;
	AttackCooldown = 1.35f;
	ChaseAcceptanceRadius = 50.f;
	ChaseProximityBuffer = 45.f;

	AggroRadius = ScatterPhaseAggroClamp;
	CorpseLifeSpan = 2.5f;

	// 바닥에 깔린 살점 덩어리. 기본 ACharacter 캡슐(반경 34 / 반높이 88)을 그대로 쓰면 공중에 뜸.
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->InitCapsuleSize(34.f, 30.f);
	}
	if (USkeletalMeshComponent* SkelMesh = GetMesh())
	{
		// 메시 발(루트) 기준이 캡슐 바닥에 맞도록 -HalfHeight 만큼 내림.
		SkelMesh->SetRelativeLocation(FVector(0.f, 0.f, -30.f));
	}
	if (UCharacterMovementComponent* CharMove = GetCharacterMovement())
	{
		// 작은 캡슐이라 기본 step up/perch 값이 너무 높음. 기는 형상에 맞춰 낮춤.
		CharMove->MaxStepHeight = 20.f;
		CharMove->SetWalkableFloorAngle(60.f);
	}
}

void ACrawlingFleshMassEnemy::BeginPlay()
{
	Super::BeginPlay();
	BindSwarmDelegates();
}

void ACrawlingFleshMassEnemy::BindSwarmDelegates()
{
	AEnemySpawner* Spawner = OwningSwarmSpawner.Get();
	if (!IsValid(Spawner) || bSwarmDelegateBound || !HasAuthority())
	{
		return;
	}

	Spawner->OnWaveSpawnQueueEmptied.AddDynamic(this, &ACrawlingFleshMassEnemy::HandleWaveSpawnQueueEmptied);
	bSwarmDelegateBound = true;
}

void ACrawlingFleshMassEnemy::InitializeSwarmMembership(AEnemySpawner* OwningSpawner)
{
	if (!HasAuthority())
	{
		return;
	}

	OwningSwarmSpawner = OwningSpawner;
	BindSwarmDelegates();
}

void ACrawlingFleshMassEnemy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CleanupSwarmDelegates();
	Super::EndPlay(EndPlayReason);
}

void ACrawlingFleshMassEnemy::CleanupSwarmDelegates()
{
	if (AEnemySpawner* Spawner = OwningSwarmSpawner.Get())
	{
		Spawner->OnWaveSpawnQueueEmptied.RemoveDynamic(this, &ACrawlingFleshMassEnemy::HandleWaveSpawnQueueEmptied);
	}

	bSwarmDelegateBound = false;
}

void ACrawlingFleshMassEnemy::HandleWaveSpawnQueueEmptied(int32 WaveIndex, AEnemySpawner* Spawner)
{
	if (!IsAlive() || !Spawner || Spawner != OwningSwarmSpawner.Get())
	{
		return;
	}

	Spawner->OnWaveSpawnQueueEmptied.RemoveDynamic(this, &ACrawlingFleshMassEnemy::HandleWaveSpawnQueueEmptied);
	bSwarmDelegateBound = false;

	if (!bHasCachedChaseAggroRadius)
	{
		CachedChaseAggroRadius = ChaseAggroRadiusAfterScatter;
		bHasCachedChaseAggroRadius = true;
	}

	bCachedLightTracking = bEnableLightTracking;
	AggroRadius = ScatterPhaseAggroClamp;
	if (bDisableLightTrackingWhileScattering)
	{
		bEnableLightTracking = false;
	}

	bScatterChaseHoldActive = true;

	if (UWorld* World = GetWorld())
	{
		const float TimeSec = World->GetTimeSeconds();
		ScatterPhaseEndsAtWorldSeconds = TimeSec + ScatterDuration;
	}

	IssueRandomScatterDestination();
	ScatterMoveCooldownRemaining = ScatterRetargetInterval;

	if (!IsValid(TargetActor))
	{
		FindDefaultTarget();
	}

	UpdateState();
}

void ACrawlingFleshMassEnemy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority() || !IsAlive())
	{
		return;
	}

	if (ScatterPhaseEndsAtWorldSeconds > 0.f)
	{
		if (const UWorld* World = GetWorld())
		{
			if (World->GetTimeSeconds() >= ScatterPhaseEndsAtWorldSeconds)
			{
				TransitionToChasePhase();
			}
			else if (bScatterChaseHoldActive)
			{
				ScatterMoveCooldownRemaining -= DeltaSeconds;
				if (ScatterMoveCooldownRemaining <= 0.f)
				{
					IssueRandomScatterDestination();
					ScatterMoveCooldownRemaining = ScatterRetargetInterval;
				}
			}
		}
	}
}

void ACrawlingFleshMassEnemy::UpdateState()
{
	if (IsAlive() && bScatterChaseHoldActive && ScatterPhaseEndsAtWorldSeconds > 0.f)
	{
		SetEnemyState(EEnemyAIState::Idle);
		return;
	}

	Super::UpdateState();
}

void ACrawlingFleshMassEnemy::UpdateIdle(float DeltaSeconds)
{
	if (bScatterChaseHoldActive && ScatterPhaseEndsAtWorldSeconds > 0.f)
	{
		return;
	}

	Super::UpdateIdle(DeltaSeconds);
}

// 베이스 UpdateAttack은 매 틱 StopMovement 를 호출해 AttackRange 경계에서 끊김을 만듦.
// swarm 크롤러는 멈추지 않고 들이박는 연출이 자연스러우므로 추격을 유지한 채 쿨다운만 본다.
void ACrawlingFleshMassEnemy::UpdateAttack()
{
	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		if (IsValid(TargetActor))
		{
			const float Acceptance = FMath::Max(5.f, AttackRange * 0.5f);
			AI->MoveToActor(TargetActor, Acceptance, false);
		}
	}

	const UWorld* World = GetWorld();
	const float CurrentTime = World ? World->GetTimeSeconds() : 0.f;
	if (CurrentTime - LastSwarmAttackTime < AttackCooldown)
	{
		return;
	}
	LastSwarmAttackTime = CurrentTime;

	PerformAttack(TargetActor);
}

bool ACrawlingFleshMassEnemy::TryPickRandomScatterPoint(FVector const& Origin, FVector& OutLocation) const
{
	UWorld* World = GetWorld();
	if (!World || ScatterRadius <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetNavigationSystem(World);
	if (!NavSys)
	{
		return false;
	}

	FNavLocation NavLocation;
	if (NavSys->GetRandomReachablePointInRadius(Origin, ScatterRadius, NavLocation))
	{
		OutLocation = NavLocation.Location;
		return true;
	}

	const float FallbackRadius = FMath::Clamp(ScatterRadius * 0.35f, 256.f, 1024.f);
	if (NavSys->GetRandomReachablePointInRadius(GetActorLocation(), FallbackRadius, NavLocation))
	{
		OutLocation = NavLocation.Location;
		return true;
	}

	return false;
}

void ACrawlingFleshMassEnemy::IssueRandomScatterDestination()
{
	if (!bScatterChaseHoldActive || !IsAlive())
	{
		return;
	}

	AAIController* AI = Cast<AAIController>(GetController());
	if (!AI || !OwningSwarmSpawner.IsValid())
	{
		return;
	}

	const FVector Origin = OwningSwarmSpawner->GetActorLocation();
	FVector Goal;

	if (!TryPickRandomScatterPoint(Origin, Goal))
	{
		static constexpr float FallbackDistance = 300.f;
		Goal = GetActorLocation();
		Goal += FVector(
			FMath::FRandRange(-FallbackDistance, FallbackDistance),
			FMath::FRandRange(-FallbackDistance, FallbackDistance),
			0.f);
	}

	const EPathFollowingRequestResult::Type MoveResult = AI->MoveToLocation(Goal, ScatterMoveAcceptanceRadius);
	(void)MoveResult;
}

void ACrawlingFleshMassEnemy::TransitionToChasePhase()
{
	bScatterChaseHoldActive = false;
	ScatterPhaseEndsAtWorldSeconds = -1.f;
	ScatterMoveCooldownRemaining = 0.f;

	if (bHasCachedChaseAggroRadius)
	{
		AggroRadius = CachedChaseAggroRadius;
		bHasCachedChaseAggroRadius = false;
	}

	bEnableLightTracking = bCachedLightTracking;

	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		AI->StopMovement();
	}

	FindDefaultTarget();
	UpdateState();
}
