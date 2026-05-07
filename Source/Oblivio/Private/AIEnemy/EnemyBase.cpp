#include "AIEnemy/EnemyBase.h"
#include "Combat/EnemyCombatRegistrySubsystem.h"
#include "AIEnemy/EnemyAIController.h"
#include "AIController.h"
#include "OblivioGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"
#include "OblivioCharacter.h"
#include "OblivioComponents/EnemyCombatComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/World.h"

// =============================================================================
// AEnemyBase 구현 요약
// BeginPlay: 체력·이속 초기화 후 어그로/패트롤에 맞춰 첫 상태
// Tick    : 생존·빛CC 통과 시 FSM 갱신 + 상태별 MoveTo / 공격
// UpdateState: 어그로 있으면 Chase/Attack, 없으면 Investigate→Search→Patrol→Idle 우선순위
// CC: Slow/Stun 은 RefreshWalkSpeedFromSources 에서 빛 배율과 곱해 적용
// =============================================================================

// AIController 클래스 지정, 이동은 폰 회전 맞춤
AEnemyBase::AEnemyBase()
{
	PrimaryActorTick.bCanEverTick = true;

	AIControllerClass = AEnemyAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
	GetCharacterMovement()->MaxWalkSpeed = MoveSpeed;

	CombatComp = CreateDefaultSubobject<UEnemyCombatComponent>(TEXT("CombatComp"));
}

// 생존·체력·첫 FSM. 패트롤 포인트가 있으면 플레이어 없을 때 Patrol로 시작 가능.
void AEnemyBase::BeginPlay()
{
	Super::BeginPlay();

	CurrentHealth = MaxHealth;
	RefreshWalkSpeedFromSources();
	FindDefaultTarget();

	if (HasValidAggroTarget())
	{
		LastKnownTargetLocation = TargetActor->GetActorLocation();
		bHadAggroLastTick = true;
		SetEnemyState(IsTargetInAttackRange() ? EEnemyAIState::Attack : EEnemyAIState::Chase);
	}
	else if (!PatrolPoints.IsEmpty())
	{
		bHadAggroLastTick = false;
		SetEnemyState(EEnemyAIState::Patrol);
	}
	else
	{
		bHadAggroLastTick = false;
		SetEnemyState(EEnemyAIState::Idle);
	}

	// 스폰 시 기본 상태가 이미 Idle이면 SetEnemyState가 no-op → 배회 쿨 초기화
	if (EnemyState == EEnemyAIState::Idle && bEnableIdleWander)
	{
		IdleWanderRetargetCooldown = FMath::FRandRange(0.5f, 2.0f);
	}

	if (UWorld* World = GetWorld())
	{
		if (UEnemyCombatRegistrySubsystem* Reg = World->GetSubsystem<UEnemyCombatRegistrySubsystem>())
		{
			Reg->RegisterEnemy(this);
		}
	}
}

void AEnemyBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UEnemyCombatRegistrySubsystem* Reg = World->GetSubsystem<UEnemyCombatRegistrySubsystem>())
		{
			Reg->UnregisterEnemy(this);
		}
	}
	Super::EndPlay(EndPlayReason);
}

// CC 경직(외부 시스템)에 막혔을 때만 행동 정지. 빛 슬로우/정지/사망은 전투 측이 ApplyCC*/TakeDamage로 처리.
void AEnemyBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!IsAlive())
	{
		return;
	}

	if (!IsValid(TargetActor))
	{
		FindDefaultTarget();
	}

	// 경직 중에도 FSM 전이는 갱신, 이동·공격 스위치만 아래에서 생략.
	UpdateState();

	DrawAggroDebug();

	if (bCCStunned)
	{
		return;
	}

	switch (EnemyState)
	{
	case EEnemyAIState::Idle:
		UpdateIdle(DeltaSeconds);
		break;
	case EEnemyAIState::Chase:
		UpdateChase();
		break;
	case EEnemyAIState::Attack:
		UpdateAttack();
		break;
	case EEnemyAIState::Patrol:
		UpdatePatrol(DeltaSeconds);
		break;
	case EEnemyAIState::Investigate:
		UpdateInvestigate(DeltaSeconds);
		break;
	case EEnemyAIState::Search:
		UpdateSearch(DeltaSeconds);
		break;
	case EEnemyAIState::TrackLight:
		UpdateTrackLight(DeltaSeconds);
		break;
	case EEnemyAIState::Dead:
	default:
		break;
	}
}

EEnemyCCState AEnemyBase::GetCrowdControlState() const
{
	if (!IsAlive())
	{
		return EEnemyCCState::None;
	}
	if (bCCStunned)
	{
		return EEnemyCCState::Stunned;
	}
	if (bCCSlowActive)
	{
		return EEnemyCCState::Slowed;
	}
	return EEnemyCCState::None;
}

void AEnemyBase::ApplyCCSlow(float SpeedMultiplier, float Duration)
{
	if (!IsAlive())
	{
		return;
	}

	const float Clamped = FMath::Clamp(SpeedMultiplier, 0.0f, 1.0f);
	bCCSlowActive = true;
	CCSlowSpeedMultiplier = FMath::Min(CCSlowSpeedMultiplier, Clamped);
	UE_LOG(LogTemp, Warning, TEXT("ApplyCCSlow Called"));	//CombatComponent호출 체크용
	if (Duration > KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogTemp, Warning, TEXT("Setting slow timer")); //CombatComponent호출 체크용
		float NewDuration = Duration;
		if (UWorld* World = GetWorld())
		{
			const float Remaining = World->GetTimerManager().GetTimerRemaining(CCSlowTimerHandle);
			NewDuration = FMath::Max(Remaining, Duration);
		}
		GetWorldTimerManager().SetTimer(CCSlowTimerHandle, this, &AEnemyBase::OnCCSlowExpired, NewDuration, false);
	}
	else
	{
		GetWorldTimerManager().ClearTimer(CCSlowTimerHandle);
	}

	RefreshWalkSpeedFromSources();
}

void AEnemyBase::ApplyCCStun(float Duration)
{
	if (!IsAlive())
	{
		return;
	}

	bCCStunned = true;
	StopEnemyMovement();
	RefreshWalkSpeedFromSources();
	UE_LOG(LogTemp, Warning, TEXT("ApplyCCStun Called"));	//CombatComponent호출 체크
	if (Duration > KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogTemp, Warning, TEXT("Setting stun timer")); //CombatComponent호출 체크용
		float NewDuration = Duration;
		if (UWorld* World = GetWorld())
		{
			const float Remaining = World->GetTimerManager().GetTimerRemaining(CCStunTimerHandle);
			NewDuration = FMath::Max(Remaining, Duration);
		}
		GetWorldTimerManager().SetTimer(CCStunTimerHandle, this, &AEnemyBase::OnCCStunExpired, NewDuration, false);
	}
	else
	{
		GetWorldTimerManager().ClearTimer(CCStunTimerHandle);
	}
}

void AEnemyBase::ClearCCSlow()
{
	bCCSlowActive = false;
	CCSlowSpeedMultiplier = 1.0f;
	GetWorldTimerManager().ClearTimer(CCSlowTimerHandle);
	if (IsAlive())
	{
		RefreshWalkSpeedFromSources();
	}
}

void AEnemyBase::ClearCCStun()
{
	bCCStunned = false;
	GetWorldTimerManager().ClearTimer(CCStunTimerHandle);
	if (IsAlive())
	{
		RefreshWalkSpeedFromSources();
		UpdateState();
	}
}

void AEnemyBase::OnCCSlowExpired()
{
	bCCSlowActive = false;
	CCSlowSpeedMultiplier = 1.0f;
	if (IsAlive())
	{
		RefreshWalkSpeedFromSources();
	}
}

void AEnemyBase::OnCCStunExpired()
{
	bCCStunned = false;
	if (IsAlive())
	{
		RefreshWalkSpeedFromSources();
		UpdateState();
	}
}

// 피격: 부모(DamageType 처리) 후 현재 체력 반영, OnEnemyDamaged, 0 이하면 Die().
float AEnemyBase::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	const float AppliedDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	if (!IsAlive() || AppliedDamage <= 0.0f)
	{
		return 0.0f;
	}

	CurrentHealth = FMath::Max(0.0f, CurrentHealth - AppliedDamage);
	NotifyEnemyDamageApplied(AppliedDamage);
	OnEnemyDamaged.Broadcast(AppliedDamage, CurrentHealth, MaxHealth);
	if (UWorld* World = GetWorld())
	{
		if (UEnemyCombatRegistrySubsystem* Reg = World->GetSubsystem<UEnemyCombatRegistrySubsystem>())
		{
			Reg->NotifyEnemyDamaged(this, AppliedDamage, CurrentHealth, MaxHealth);
		}
	}

	if (CurrentHealth <= 0.0f)
	{
		Die();
	}

	return AppliedDamage;
}

void AEnemyBase::NotifyEnemyDamageApplied(float /*AppliedDamage*/)
{
}

// 손전등 피격 판단만 수행. 빛 피해·둔화·정지·사망 처리는 전투 시스템에서 담당.
void AEnemyBase::OnLightHit(float Intensity, float Duration)
{
	if (!IsAlive())
	{
		return;
	}

	const float ClampedDuration = FMath::Max(0.0f, Duration);
	const float ClampedIntensity = FMath::Clamp(Intensity, 0.0f, 1.0f);
	OnEnemyLightHit.Broadcast(this, ClampedIntensity, ClampedDuration);
}

static void BroadcastEnemyDamageToRegistry(AEnemyBase* Self, float Amount, float Cur, float Max)
{
	Self->OnEnemyDamaged.Broadcast(Amount, Cur, Max);
	if (UWorld* World = Self->GetWorld())
	{
		if (UEnemyCombatRegistrySubsystem* Reg = World->GetSubsystem<UEnemyCombatRegistrySubsystem>())
		{
			Reg->NotifyEnemyDamaged(Self, Amount, Cur, Max);
		}
	}
}

void AEnemyBase::Heal(float Amount)
{
	if (!IsAlive() || Amount <= 0.f)
	{
		return;
	}
	const float Old = CurrentHealth;
	CurrentHealth = FMath::Min(MaxHealth, CurrentHealth + Amount);
	const float Delta = CurrentHealth - Old;
	if (Delta > 0.f)
	{
		// 회복은 음수 형태로 브로드캐스트(부호로 구분).
		BroadcastEnemyDamageToRegistry(this, -Delta, CurrentHealth, MaxHealth);
	}
}

void AEnemyBase::SetCurrentHealth(float NewHealth)
{
	if (EnemyState == EEnemyAIState::Dead)
	{
		return;
	}
	const float Old = CurrentHealth;
	CurrentHealth = FMath::Clamp(NewHealth, 0.f, MaxHealth);
	const float Delta = Old - CurrentHealth;
	if (FMath::Abs(Delta) > KINDA_SMALL_NUMBER)
	{
		BroadcastEnemyDamageToRegistry(this, Delta, CurrentHealth, MaxHealth);
	}
	if (CurrentHealth <= 0.f)
	{
		Die();
	}
}

void AEnemyBase::KillEnemy()
{
	if (EnemyState == EEnemyAIState::Dead)
	{
		return;
	}
	Die();
}

// 수동으로 추적 대상 교체 후 FSM 즉시 갱신
void AEnemyBase::SetTargetActor(AActor* NewTarget)
{
	TargetActor = NewTarget;
	OnEnemyTargetChanged.Broadcast(this, NewTarget);
	UpdateState();
}

// 어그로 대상이 없을 때만: 자극 위치·타입으로 Investigate 큐 적재
void AEnemyBase::ReportStimulus(FVector WorldLocation, EEnemyStimulusType StimulusType)
{
	if (!IsAlive())
	{
		return;
	}
	if (HasValidAggroTarget())
	{
		return;
	}

	PendingInvestigateLocation = WorldLocation;
	LastReportedStimulusType = StimulusType;
	bHasPendingInvestigate = true;
	InvestigateTimerRemaining = InvestigateStimulusTimeout;
	OnEnemyStimulusReported.Broadcast(this, WorldLocation, StimulusType);
	UpdateState();
}

// 월드 플레이어 0번 폰( Character 아님 포함). 없으면 타겟 없음 → 어그로 실패 가능
void AEnemyBase::FindDefaultTarget()
{
	TargetActor = UGameplayStatics::GetPlayerPawn(this, 0);
}

// 유효 폰 + AggroRadius(0이면 거리 무시). 수평 전용이면 Z 무시.
bool AEnemyBase::HasValidAggroTarget() const
{
	if (!IsValid(TargetActor))
	{
		return false;
	}
	if (AggroRadius <= 0.0f)
	{
		return true;
	}

	const FVector A = GetActorLocation();
	const FVector B = TargetActor->GetActorLocation();
	const float DistSq = bAggroUseHorizontalDistance
		? FVector::DistSquared(FVector(A.X, A.Y, 0.0f), FVector(B.X, B.Y, 0.0f))
		: FVector::DistSquared(A, B);

	return DistSq <= FMath::Square(AggroRadius);
}

// 어그로 → Chase/Attack, 없으면 TrackLight(옵션) → 방금 놓침 Search → 자극 Investigate → …
void AEnemyBase::UpdateState()
{
	if (!IsAlive())
	{
		SetEnemyState(EEnemyAIState::Dead);
		return;
	}

	const bool bAggro = HasValidAggroTarget();

	if (bAggro)
	{
		ClearLightTrackState();
		LastKnownTargetLocation = TargetActor->GetActorLocation();
		SearchTimeRemaining = 0.0f;
		SearchRetargetCooldown = 0.0f;
		bHasPendingInvestigate = false;
		SetEnemyState(IsTargetInAttackRange() ? EEnemyAIState::Attack : EEnemyAIState::Chase);
		bHadAggroLastTick = true;
		return;
	}

	if (EnemyState == EEnemyAIState::TrackLight)
	{
		return;
	}

	if (bHadAggroLastTick && SearchPhaseDuration > 0.0f)
	{
		SearchTimeRemaining = SearchPhaseDuration;
		SearchAnchor = LastKnownTargetLocation;
		SearchRetargetCooldown = 0.0f;
	}
	bHadAggroLastTick = false;

	if (bEnableLightTracking)
	{
		FVector Goal;
		if (TryComputeFlashlightTrackGoal(Goal))
		{
			LightTrackGoalWorld = Goal;
			bLightTrackGoalValid = true;
			bLightTrackSealed = false;
			SetEnemyState(EEnemyAIState::TrackLight);
			return;
		}
	}

	if (bHasPendingInvestigate)
	{
		SetEnemyState(EEnemyAIState::Investigate);
		return;
	}

	if (SearchTimeRemaining > 0.0f)
	{
		SetEnemyState(EEnemyAIState::Search);
		return;
	}

	if (!PatrolPoints.IsEmpty())
	{
		SetEnemyState(EEnemyAIState::Patrol);
		return;
	}

	SetEnemyState(EEnemyAIState::Idle);
}

void AEnemyBase::ClearLightTrackState()
{
	bLightTrackGoalValid = false;
	bLightTrackSealed = false;
	LightTrackGoalWorld = FVector::ZeroVector;
	LightTrackGraceRemaining = 0.0f;
}

USpotLightComponent* AEnemyBase::ResolveFlashlightSpotForTracking() const
{
	if (!IsValid(TargetActor))
	{
		return nullptr;
	}
	if (AOblivioCharacter* const Oc = Cast<AOblivioCharacter>(TargetActor))
	{
		if (Oc->FlashlightComponent)
		{
			return Oc->FlashlightComponent;
		}
	}
	return TargetActor->FindComponentByClass<USpotLightComponent>();
}

bool AEnemyBase::IsFlashlightTrackSourceOff(USpotLightComponent* Spot) const
{
	if (!Spot)
	{
		return true;
	}
	if (const AOblivioCharacter* const Player = Cast<AOblivioCharacter>(TargetActor))
	{
		return !Player->bIsFlashlightOn || Player->Battery <= 0.0f;
	}
	return !Spot->IsVisible() || Spot->Intensity <= KINDA_SMALL_NUMBER;
}

bool AEnemyBase::PassesLightTrackFrontFaceTest(const FVector& LightWorldLocation) const
{
	if (!bLightTrackRequireFrontFace)
	{
		return true;
	}
	const FVector ToLight = (LightWorldLocation - GetActorLocation()).GetSafeNormal();
	if (ToLight.IsNearlyZero())
	{
		return false;
	}
	return FVector::DotProduct(GetActorForwardVector(), ToLight) >= LightTrackFrontFaceMinDot;
}

bool AEnemyBase::TryComputeFlashlightTrackGoal(FVector& OutGoal)
{
	USpotLightComponent* const Spot = ResolveFlashlightSpotForTracking();
	if (!Spot)
	{
		return false;
	}

	if (IsFlashlightTrackSourceOff(Spot))
	{
		return false;
	}

	const FVector TestPoint = GetActorLocation() + FVector(0.0f, 0.0f, LightTrackConeTestZ);
	const FVector Origin = Spot->GetComponentLocation();
	FVector Dir = Spot->GetForwardVector().GetSafeNormal();
	const FVector ToPoint = TestPoint - Origin;
	const float Dist = ToPoint.Size();

	// 손전등에 거의 붙은 경우에도 목표는 유지(이전엔 실패 → seal 로 한 번만 추적되던 원인).
	const FVector RawGoal = Origin;
	if (Dist < KINDA_SMALL_NUMBER)
	{
		if (!PassesLightTrackFrontFaceTest(Origin))
		{
			return false;
		}
		UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
		if (!NavSys)
		{
			OutGoal = RawGoal;
			return true;
		}
		FNavLocation Projected;
		if (NavSys->ProjectPointToNavigation(RawGoal, Projected, FVector(400.0f, 400.0f, 500.0f)))
		{
			OutGoal = Projected.Location;
		}
		else
		{
			OutGoal = RawGoal;
		}
		return true;
	}

	const float MaxDist = Spot->AttenuationRadius + LightTrackConeRadiusSlack;
	if (Dist > MaxDist)
	{
		return false;
	}

	const FVector ToNorm = ToPoint / Dist;
	const float HalfOuterDeg = Spot->OuterConeAngle * 0.5f;
	const float CosCone = FMath::Cos(FMath::DegreesToRadians(HalfOuterDeg));
	// 손전등은 단방향 — Dir과 같은 반구가 아니거나(콘 뒤쪽), 콘 각도를 벗어나면 콘 밖.
	// 이전엔 Dir = -Dir 로 뒤집어 검사하느라 ‘플레이어 등 뒤’의 적이 추적 진입하는 버그 발생.
	if (FVector::DotProduct(Dir, ToNorm) < CosCone - KINDA_SMALL_NUMBER)
	{
		return false;
	}

	if (!PassesLightTrackFrontFaceTest(Origin))
	{
		return false;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSys)
	{
		OutGoal = RawGoal;
		return true;
	}

	FNavLocation Projected;
	if (NavSys->ProjectPointToNavigation(RawGoal, Projected, FVector(400.0f, 400.0f, 500.0f)))
	{
		OutGoal = Projected.Location;
	}
	else
	{
		OutGoal = RawGoal;
	}
	return true;
}

void AEnemyBase::UpdateTrackLight(float DeltaSeconds)
{
	if (!bEnableLightTracking)
	{
		ClearLightTrackState();
		SetEnemyState(EEnemyAIState::Idle);
		UpdateState();
		return;
	}

	AAIController* const AI = Cast<AAIController>(GetController());
	if (!AI)
	{
		return;
	}

	if (HasValidAggroTarget())
	{
		ClearLightTrackState();
		UpdateState();
		return;
	}

	USpotLightComponent* const Spot = ResolveFlashlightSpotForTracking();

	// 콘·정면·거리 모두 만족할 때만 살아 있는 추적. 성공 시에만 MoveTo(매 틱 목표 갱신).
	FVector NewGoal;
	if (TryComputeFlashlightTrackGoal(NewGoal))
	{
		bLightTrackSealed = false;
		LightTrackGoalWorld = NewGoal;
		bLightTrackGoalValid = true;
		LightTrackGraceRemaining = LightTrackLossGracePeriod;
		const EPathFollowingRequestResult::Type MoveResult = AI->MoveToLocation(LightTrackGoalWorld, LightTrackAcceptanceRadius);
		if (MoveResult == EPathFollowingRequestResult::Failed)
		{
			UE_LOG(LogTemp, Verbose, TEXT("%s TrackLight MoveToLocation failed"), *GetNameSafe(this));
		}
		return;
	}

	// TryCompute 실패
	if (IsFlashlightTrackSourceOff(Spot))
	{
		// 손전등 꺼짐·배터리 등: 마지막으로 “유효했던” 목표까지만 밀봉 추적
		if (!bLightTrackSealed)
		{
			bLightTrackSealed = true;
		}
		if (!bLightTrackGoalValid)
		{
			ClearLightTrackState();
			StopEnemyMovement();
			SetEnemyState(EEnemyAIState::Idle);
			UpdateState();
			return;
		}

		const float DistSq = FVector::DistSquared(GetActorLocation(), LightTrackGoalWorld);
		if (DistSq <= FMath::Square(LightTrackAcceptanceRadius))
		{
			ClearLightTrackState();
			StopEnemyMovement();
			SetEnemyState(EEnemyAIState::Idle);
			UpdateState();
			return;
		}

		const EPathFollowingRequestResult::Type MoveResult = AI->MoveToLocation(LightTrackGoalWorld, LightTrackAcceptanceRadius);
		if (MoveResult == EPathFollowingRequestResult::Failed)
		{
			UE_LOG(LogTemp, Verbose, TEXT("%s TrackLight sealed repath failed"), *GetNameSafe(this));
		}
		return;
	}

	// 손전등은 켜져 있는데 지금 한 프레임만 콘/정면 조건이 빠진 상황 —
	// 즉시 Idle로 떨어지면 손전등이 켜져 있는데 추격이 끊기는 깜박임이 발생하므로 잠시 잔여 시간만큼 LastGoal 추적 유지.
	if (LightTrackGraceRemaining > 0.0f && bLightTrackGoalValid)
	{
		LightTrackGraceRemaining = FMath::Max(0.0f, LightTrackGraceRemaining - DeltaSeconds);

		const float DistSq = FVector::DistSquared(GetActorLocation(), LightTrackGoalWorld);
		if (DistSq <= FMath::Square(LightTrackAcceptanceRadius))
		{
			ClearLightTrackState();
			StopEnemyMovement();
			SetEnemyState(EEnemyAIState::Idle);
			UpdateState();
			return;
		}

		const EPathFollowingRequestResult::Type MoveResult = AI->MoveToLocation(LightTrackGoalWorld, LightTrackAcceptanceRadius);
		if (MoveResult == EPathFollowingRequestResult::Failed)
		{
			UE_LOG(LogTemp, Verbose, TEXT("%s TrackLight grace repath failed"), *GetNameSafe(this));
		}
		return;
	}

	// Grace 소진: 진짜로 추적 해제
	bLightTrackSealed = false;
	StopEnemyMovement();
	ClearLightTrackState();
	SetEnemyState(EEnemyAIState::Idle);
	UpdateState();
}

// NavMesh MoveToActor로 플레이어 접근
void AEnemyBase::UpdateChase()
{
	AAIController* EnemyController = Cast<AAIController>(GetController());
	if (!EnemyController || !IsValid(TargetActor))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s cannot chase: missing controller or target"), *GetNameSafe(this));
		return;
	}

	// Chase 수용이 AttackRange에 너무 가깝거나 크면 미세하게 밖에서 멈춰 Attack 미전환 → AttackRange - ChaseProximityBuffer 로 상한.
	const float MaxAcceptanceForAttack = FMath::Max(1.0f, AttackRange - ChaseProximityBuffer);
	const float EffectiveChaseAcceptance = FMath::Min(ChaseAcceptanceRadius, MaxAcceptanceForAttack);

	const EPathFollowingRequestResult::Type MoveResult = EnemyController->MoveToActor(TargetActor, EffectiveChaseAcceptance);
	if (MoveResult == EPathFollowingRequestResult::Failed)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s failed to request path to %s"), *GetNameSafe(this), *GetNameSafe(TargetActor));
	}
}

// 정지 후 쿨다운 맞으면 PerformAttack (BP 오버라이드 가능)
void AEnemyBase::UpdateAttack()
{
	StopEnemyMovement();

	const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (CurrentTime - LastAttackTime < AttackCooldown)
	{
		return;
	}

	LastAttackTime = CurrentTime;
	PerformAttack(TargetActor);
}

// 기본 근접: 범위 내 공격 가능 판단만 브로드캐스트. 실제 공격 방식은 전투 시스템/BP에서 담당.
void AEnemyBase::PerformAttack_Implementation(AActor* Target)
{
	if (!IsValid(Target) || !IsTargetInAttackRange())
	{
		return;
	}

	OnEnemyAttackCommitted.Broadcast(this, Target, AttackDamage);
	if (UWorld* World = GetWorld())
	{
		if (UEnemyCombatRegistrySubsystem* Reg = World->GetSubsystem<UEnemyCombatRegistrySubsystem>())
		{
			Reg->NotifyEnemyAttackCommitted(this, Target, AttackDamage);
		}
	}
}

// PatrolPoints 순서대로 도착 반경 안 들어오면 다음 지점
void AEnemyBase::UpdatePatrol(float DeltaSeconds)
{
	(void)DeltaSeconds;

	if (PatrolPoints.IsEmpty())
	{
		return;
	}

	AAIController* AI = Cast<AAIController>(GetController());
	if (!AI)
	{
		return;
	}

	for (int32 Guard = 0; Guard < PatrolPoints.Num(); ++Guard)
	{
		AActor* const Point = PatrolPoints[CurrentPatrolIndex];
		if (IsValid(Point))
		{
			if (FVector::DistSquared(GetActorLocation(), Point->GetActorLocation()) <= FMath::Square(PatrolAcceptanceRadius))
			{
				CurrentPatrolIndex = (CurrentPatrolIndex + 1) % PatrolPoints.Num();
				continue;
			}

			const EPathFollowingRequestResult::Type MoveResult = AI->MoveToActor(Point, PatrolAcceptanceRadius);
			if (MoveResult == EPathFollowingRequestResult::Failed)
			{
				UE_LOG(LogTemp, Verbose, TEXT("%s patrol MoveToActor failed"), *GetNameSafe(this));
			}
			return;
		}

		CurrentPatrolIndex = (CurrentPatrolIndex + 1) % PatrolPoints.Num();
	}
}

// 어그로 밖·Patrol 없음: 주변 Nav 랜덤 지점으로 배회
void AEnemyBase::UpdateIdle(float DeltaSeconds)
{
	(void)DeltaSeconds;

	if (!bEnableIdleWander || HasValidAggroTarget())
	{
		StopEnemyMovement();
		return;
	}

	AAIController* AI = Cast<AAIController>(GetController());
	if (!AI)
	{
		return;
	}

	IdleWanderRetargetCooldown -= DeltaSeconds;
	if (IdleWanderRetargetCooldown > 0.0f)
	{
		return;
	}

	IdleWanderRetargetCooldown = IdleWanderRetargetInterval;

	UWorld* World = GetWorld();
	UNavigationSystemV1* NavSys = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
	if (!NavSys)
	{
		IdleWanderRetargetCooldown = FMath::Min(IdleWanderRetargetCooldown, 1.0f);
		return;
	}

	const FVector Origin = GetActorLocation();
	const FVector2D Offset2D = FMath::RandPointInCircle(IdleWanderRadius);
	const FVector Candidate = Origin + FVector(Offset2D.X, Offset2D.Y, 0.0f);

	FNavLocation Projected;
	if (NavSys->ProjectPointToNavigation(Candidate, Projected, FVector(250.0f, 250.0f, 300.0f)))
	{
		const EPathFollowingRequestResult::Type MoveResult = AI->MoveToLocation(Projected.Location, IdleWanderAcceptanceRadius);
		if (MoveResult == EPathFollowingRequestResult::Failed)
		{
			UE_LOG(LogTemp, Verbose, TEXT("%s idle wander MoveToLocation failed"), *GetNameSafe(this));
			IdleWanderRetargetCooldown = FMath::Min(1.0f, IdleWanderRetargetInterval * 0.25f);
		}
	}
	else
	{
		IdleWanderRetargetCooldown = FMath::Min(1.0f, IdleWanderRetargetInterval * 0.25f);
	}
}

// 자극 지점까지 MoveToLocation, 도착/타임아웃 시 큐 해제 후 UpdateState
void AEnemyBase::UpdateInvestigate(float DeltaSeconds)
{
	AAIController* AI = Cast<AAIController>(GetController());
	if (!AI)
	{
		return;
	}

	const float DistSq = FVector::DistSquared(GetActorLocation(), PendingInvestigateLocation);
	if (DistSq <= FMath::Square(InvestigateAcceptanceRadius))
	{
		bHasPendingInvestigate = false;
		InvestigateTimerRemaining = 0.0f;
		UpdateState();
		return;
	}

	InvestigateTimerRemaining -= DeltaSeconds;
	if (InvestigateTimerRemaining <= 0.0f)
	{
		bHasPendingInvestigate = false;
		UpdateState();
		return;
	}

	const EPathFollowingRequestResult::Type MoveResult = AI->MoveToLocation(PendingInvestigateLocation, InvestigateAcceptanceRadius);
	if (MoveResult == EPathFollowingRequestResult::Failed)
	{
		UE_LOG(LogTemp, Verbose, TEXT("%s investigate MoveToLocation failed"), *GetNameSafe(this));
	}
}

// 어그로 상실 후: SearchAnchor 주변 랜덤 점을 주기적으로 재요청
void AEnemyBase::UpdateSearch(float DeltaSeconds)
{
	SearchTimeRemaining -= DeltaSeconds;
	if (SearchTimeRemaining <= 0.0f)
	{
		SearchTimeRemaining = 0.0f;
		UpdateState();
		return;
	}

	SearchRetargetCooldown -= DeltaSeconds;
	if (SearchRetargetCooldown > 0.0f)
	{
		return;
	}

	SearchRetargetCooldown = SearchRetargetInterval;

	AAIController* AI = Cast<AAIController>(GetController());
	if (!AI)
	{
		return;
	}

	const FVector2D Offset2D = FMath::RandPointInCircle(SearchRadius);
	const FVector Dest = SearchAnchor + FVector(Offset2D.X, Offset2D.Y, 0.0f);
	const EPathFollowingRequestResult::Type MoveResult = AI->MoveToLocation(Dest, PatrolAcceptanceRadius);
	if (MoveResult == EPathFollowingRequestResult::Failed)
	{
		UE_LOG(LogTemp, Verbose, TEXT("%s search MoveToLocation failed"), *GetNameSafe(this));
	}
}

// Dead 상태·충돌 끔·이벤트·시체 잔존 시간 후 파괴
void AEnemyBase::Die()
{
	if (EnemyState == EEnemyAIState::Dead)
	{
		return;
	}

	ClearLightTrackState();
	CurrentHealth = 0.0f;
	SetEnemyState(EEnemyAIState::Dead);
	StopEnemyMovement();

	GetWorldTimerManager().ClearTimer(CCSlowTimerHandle);
	GetWorldTimerManager().ClearTimer(CCStunTimerHandle);
	bCCSlowActive = false;
	bCCStunned = false;
	CCSlowSpeedMultiplier = 1.0f;

	GetCharacterMovement()->DisableMovement();
	SetActorEnableCollision(false);
	OnEnemyDied.Broadcast(this);
	if (UWorld* World = GetWorld())
	{
		if (UEnemyCombatRegistrySubsystem* Reg = World->GetSubsystem<UEnemyCombatRegistrySubsystem>())
		{
			Reg->NotifyEnemyDied(this);
		}
	}

	SetLifeSpan(FMath::Max(0.0f, CorpseLifeSpan));

	if (AOblivioGameMode* GM = Cast<AOblivioGameMode>(UGameplayStatics::GetGameMode(GetWorld())))
	{
		GM->AddMonsterKill();
	}
}

// 동일 상태면 무시(로그 스팸 방지). Verbose 로그만 출력
void AEnemyBase::SetEnemyState(EEnemyAIState NewState)
{
	if (EnemyState == NewState)
	{
		return;
	}

	const EEnemyAIState OldState = EnemyState;
	EnemyState = NewState;
	if (NewState == EEnemyAIState::Idle && bEnableIdleWander)
	{
		IdleWanderRetargetCooldown = FMath::FRandRange(0.3f, 1.5f);
	}
	NotifyEnemyStateChanged(OldState, NewState);
	OnEnemyFSMStateChanged.Broadcast(this, OldState, NewState);
	if (UWorld* World = GetWorld())
	{
		if (UEnemyCombatRegistrySubsystem* Reg = World->GetSubsystem<UEnemyCombatRegistrySubsystem>())
		{
			Reg->NotifyEnemyFSMStateChanged(this, OldState, NewState);
		}
	}
	if (NewState == EEnemyAIState::TrackLight && OldState != EEnemyAIState::TrackLight)
	{
		OnEnemyTrackLightPhase.Broadcast(this, true);
	}
	else if (OldState == EEnemyAIState::TrackLight && NewState != EEnemyAIState::TrackLight)
	{
		OnEnemyTrackLightPhase.Broadcast(this, false);
	}
	UE_LOG(LogTemp, Verbose, TEXT("%s state changed to %s"), *GetNameSafe(this), *UEnum::GetValueAsString(EnemyState));
}

// AttackRange 제곱 거리로 근접 판정
bool AEnemyBase::IsTargetInAttackRange() const
{
	return IsValid(TargetActor) && FVector::DistSquared(GetActorLocation(), TargetActor->GetActorLocation()) <= FMath::Square(AttackRange);
}

// AI 이동 요청 취소(공격/정지 시)
void AEnemyBase::StopEnemyMovement()
{
	if (AAIController* EnemyController = Cast<AAIController>(GetController()))
	{
		EnemyController->StopMovement();
	}
}

void AEnemyBase::RefreshWalkSpeedFromSources()
{
	if (!IsAlive() || !GetCharacterMovement())
	{
		return;
	}

	if (bCCStunned)
	{
		GetCharacterMovement()->MaxWalkSpeed = 0.0f;
		return;
	}

	const float CCSlowMult = bCCSlowActive ? CCSlowSpeedMultiplier : 1.0f;
	const float BaseSpeed = GetLocomotionBaseSpeed();
	GetCharacterMovement()->MaxWalkSpeed = BaseSpeed * CCSlowMult;
}

float AEnemyBase::GetLocomotionBaseSpeed() const
{
	const bool bCombatLocomotion =
		EnemyState == EEnemyAIState::Chase ||
		EnemyState == EEnemyAIState::Attack ||
		EnemyState == EEnemyAIState::TrackLight;
	if (bCombatLocomotion && ChaseMoveSpeed > KINDA_SMALL_NUMBER)
	{
		return ChaseMoveSpeed;
	}
	return MoveSpeed;
}

void AEnemyBase::DrawAggroDebug()
{
#if UE_BUILD_SHIPPING
	return;
#else
	if (!bDebugDrawAggroRadius || AggroRadius <= 0.0f || !GetWorld())
	{
		return;
	}

	if (!IsValid(TargetActor))
	{
		FindDefaultTarget();
	}

	const bool bPausedAi = bCCStunned;
	const bool bInRange = HasValidAggroTarget();

	FColor Color;
	if (bPausedAi && bInRange)
	{
		Color = FColor::Yellow;
	}
	else if (bPausedAi)
	{
		Color = FColor::Cyan;
	}
	else if (bInRange)
	{
		Color = FColor::Green;
	}
	else
	{
		Color = FColor::Orange;
	}

	DrawDebugSphere(GetWorld(), GetActorLocation(), AggroRadius, 28, Color, false, 0.0f, 0, 1.5f);

	const FVector TextPos = GetActorLocation() + FVector(0.f, 0.f, AggroRadius * 0.2f + 50.f);
	const FString Line = FString::Printf(
		TEXT("%s | Aggro:%s%s"),
		*UEnum::GetValueAsString(EnemyState),
		bInRange ? TEXT("IN") : TEXT("OUT"),
		bPausedAi ? TEXT(" | Move:PAUSED") : TEXT(""));
	DrawDebugString(GetWorld(), TextPos, Line, nullptr, Color, 0.0f, true, 1.05f);
#endif
}

void AEnemyBase::SetEnemySoundVolumeMultiplier(float NewMultiplier)
{
	EnemySoundVolumeMultiplier = FMath::Clamp(NewMultiplier, 0.0f, 4.0f);
	ApplyEnemySoundVolumes();
}

void AEnemyBase::ApplyEnemySoundVolumes()
{
}

void AEnemyBase::ApplyHealth(float Damage) {	//전투 컴포넌트 체력 업데이트용
	if (EnemyState == EEnemyAIState::Dead || Damage <= 0.0f)
	{
		return;
	}

	CurrentHealth = FMath::Max(0.0f, CurrentHealth - Damage);
	NotifyEnemyDamageApplied(Damage);
	BroadcastEnemyDamageToRegistry(this, Damage, CurrentHealth, MaxHealth);

	if (CurrentHealth <= 0.0f)
	{
		Die();
	}
}