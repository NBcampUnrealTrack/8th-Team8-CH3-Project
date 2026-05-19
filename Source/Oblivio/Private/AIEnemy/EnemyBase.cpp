#include "AIEnemy/EnemyBase.h"
#include "Combat/EnemyCombatRegistrySubsystem.h"
#include "AIEnemy/EnemyAIController.h"
#include "AIController.h"
#include "OblivioGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/DamageType.h"
#include "Engine/DamageEvents.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"
#include "Combat/LightDamageType.h"
#include "OblivioCharacter.h"
#include "OblivioComponents/EnemyCombatComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/World.h"
#include "Crafting/ObstacleBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraComponent.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

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

	IdleChaseLocomotionAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("IdleChaseLocomotionAudio"));
	if (IdleChaseLocomotionAudioComponent)
	{
		IdleChaseLocomotionAudioComponent->SetupAttachment(RootComponent);
		IdleChaseLocomotionAudioComponent->bAutoActivate = false;
		IdleChaseLocomotionAudioComponent->bStopWhenOwnerDestroyed = true;
	}

	MeleeAttackRangeIndicatorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeleeAttackRangeIndicator"));
	if (MeleeAttackRangeIndicatorMesh)
	{
		MeleeAttackRangeIndicatorMesh->SetupAttachment(RootComponent);
		MeleeAttackRangeIndicatorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MeleeAttackRangeIndicatorMesh->SetGenerateOverlapEvents(false);
		MeleeAttackRangeIndicatorMesh->SetCastShadow(false);
		MeleeAttackRangeIndicatorMesh->SetHiddenInGame(true);
		MeleeAttackRangeIndicatorMesh->SetVisibility(false);

		static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderAsset(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		if (CylinderAsset.Succeeded())
		{
			MeleeAttackRangeIndicatorMesh->SetStaticMesh(CylinderAsset.Object);
		}
	}

	HitBurnNiagaraComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("HitBurnNiagara"));
	if (HitBurnNiagaraComponent)
	{
		HitBurnNiagaraComponent->SetupAttachment(GetMesh());
		HitBurnNiagaraComponent->bAutoActivate = false;
	}
}

EEnemyAIState AEnemyBase::GetEnemyState() const
{
	return bCCStunned ? EEnemyAIState::Stunned : EnemyState;
}

// 생존·체력·첫 FSM. 패트롤 포인트가 있으면 플레이어 없을 때 Patrol로 시작 가능.
void AEnemyBase::BeginPlay()
{
	Super::BeginPlay();

	if (IdleChaseLocomotionAudioComponent)
	{
		IdleChaseLocomotionAudioComponent->OnAudioFinished.RemoveDynamic(
			this, &AEnemyBase::OnIdleChaseLocomotionAmbientPlaybackFinished);
		IdleChaseLocomotionAudioComponent->OnAudioFinished.AddDynamic(
			this, &AEnemyBase::OnIdleChaseLocomotionAmbientPlaybackFinished);
	}

	CurrentHealth = MaxHealth;
	RefreshWalkSpeedFromSources();
	FindDefaultTarget();

	LastChaseRequestedTargetPos = FVector::ZeroVector;
	LastStuckCheckLocation = GetActorLocation();
	StuckCheckTimer = 0.0f;
	StuckCounter = 0;

	if (HasValidAggroTarget())
	{
		LastKnownTargetLocation = TargetActor->GetActorLocation();
		bHadAggroLastTick = true;
		SetEnemyState(SelectStateWhileAggroed());
	}
	else if (bEnableWandering && !PatrolPoints.IsEmpty())
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
	if (EnemyState == EEnemyAIState::Idle && bEnableWandering && bEnableIdleWander)
	{
		IdleWanderRetargetCooldown = FMath::FRandRange(0.5f, 2.0f);
	}

	if (HitBurnNiagaraComponent)
	{
		if (USkeletalMeshComponent* const Skel = GetMesh())
		{
			if (!HitBurnNiagaraAttachSocketName.IsNone()
				&& Skel->DoesSocketExist(HitBurnNiagaraAttachSocketName))
			{
				HitBurnNiagaraComponent->AttachToComponent(
					Skel,
					FAttachmentTransformRules::SnapToTargetIncludingScale,
					HitBurnNiagaraAttachSocketName);
			}
		}
		if (HitBurnNiagaraSystem)
		{
			HitBurnNiagaraComponent->SetAsset(HitBurnNiagaraSystem);
		}
	}

	UpdateIdleChaseLocomotionAmbientForFsmState(GetEnemyState());

	if (MeleeAttackRangeIndicatorMesh && MeleeAttackRangeIndicatorMaterial)
	{
		MeleeAttackRangeIndicatorMesh->SetMaterial(0, MeleeAttackRangeIndicatorMaterial);
	}
	if (MeleeAttackRangeIndicatorMesh)
	{
		MeleeAttackRangeIndicatorMesh->SetVisibility(false);
		MeleeAttackRangeIndicatorMesh->SetHiddenInGame(true);
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
		World->GetTimerManager().ClearTimer(IdleChaseLocomotionAmbientFadeTimerHandle);
	}
	if (IdleChaseLocomotionAudioComponent)
	{
		IdleChaseLocomotionAudioComponent->OnAudioFinished.RemoveDynamic(
			this, &AEnemyBase::OnIdleChaseLocomotionAmbientPlaybackFinished);

		const EAudioComponentPlayState PS = IdleChaseLocomotionAudioComponent->GetPlayState();
		if (PS == EAudioComponentPlayState::Playing || PS == EAudioComponentPlayState::FadingIn
			|| PS == EAudioComponentPlayState::FadingOut)
		{
			IdleChaseLocomotionAudioComponent->FadeOut(
				FMath::Max(0.05f, IdleChaseLocomotionAmbientFadeOutDuration),
				0.f,
				EAudioFaderCurve::Linear);
		}
	}

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

	UpdateMeleeAttackRangeIndicatorVisual();

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
	DrawDebugCombatExtras();

	if (bCCStunned)
	{
		return;
	}

	if (ShouldSuppressAILocomotion())
	{
		StopEnemyMovement();
		return;
	}

	// Tank 심작 등: 파생의 GetEnemyState()(복제 안 되는 EnemyState 보정 포함)와 스위치 일치
	switch (GetEnemyState())
	{
	case EEnemyAIState::Idle:
		UpdateIdle(DeltaSeconds);
		break;
	case EEnemyAIState::Chase:
		CheckAndRecoverFromStuck(DeltaSeconds);
		HandleBlockingObstacle(DeltaSeconds);
		if (!IsValid(BlockingObstacle))
		{
			UpdateChase();
		}
		break;
	case EEnemyAIState::Attack:
		UpdateAttack();
		break;
	case EEnemyAIState::Heartbeat:
		break;
	case EEnemyAIState::JumpAttack:
		break;
	case EEnemyAIState::PlacentaDefense:
		break;
	case EEnemyAIState::Membrane:
		CheckAndRecoverFromStuck(DeltaSeconds);
		HandleBlockingObstacle(DeltaSeconds);
		if (!IsValid(BlockingObstacle))
		{
			UpdateChase();
		}
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
	ClearMeleeAttackSwingStateLock();
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
	bLastDamageWasLight = DamageEvent.DamageTypeClass &&
		DamageEvent.DamageTypeClass->IsChildOf(ULightDamageType::StaticClass());

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
	else if (HasAuthority())
	{
		NotifyStickyAggroIfPlayerDamagedBeyondRange(AppliedDamage, EventInstigator, DamageCauser);
	}

	return AppliedDamage;
}

void AEnemyBase::NotifyStickyAggroIfPlayerDamagedBeyondRange(float /*AppliedDamage*/, AController const* /*EventInstigator*/,
	AActor const* /*DamageCauser*/)
{
}

bool AEnemyBase::IsLikelyPlayerDamageCauser(AController const* EventInstigator, AActor const* DamageCauser)
{
	if (EventInstigator && EventInstigator->IsPlayerController())
	{
		return true;
	}

	if (!DamageCauser)
	{
		return false;
	}

	if (const APawn* const InstigatedBy = DamageCauser->GetInstigator())
	{
		if (InstigatedBy->IsPlayerControlled())
		{
			return true;
		}
	}

	for (AActor const* Cursor = DamageCauser; Cursor; Cursor = Cursor->GetOwner())
	{
		if (Cursor->IsA(APlayerController::StaticClass()))
		{
			return true;
		}
		if (Cursor->IsA(AOblivioCharacter::StaticClass()))
		{
			return true;
		}
	}
	return false;
}

APawn* AEnemyBase::ResolveLikelyPlayerPawnDamageCause(AController const* EventInstigator, AActor const* DamageCauser)
{
	if (!IsLikelyPlayerDamageCauser(EventInstigator, DamageCauser))
	{
		return nullptr;
	}

	if (EventInstigator)
	{
		if (const APlayerController* const PC = Cast<APlayerController>(EventInstigator))
		{
			return PC->GetPawn();
		}
	}

	if (DamageCauser)
	{
		if (APawn* const Instigated = DamageCauser->GetInstigator(); IsValid(Instigated) && Instigated->IsPlayerControlled())
		{
			return Instigated;
		}

		for (AActor const* Cursor = DamageCauser; Cursor; Cursor = Cursor->GetOwner())
		{
			if (AOblivioCharacter* const Obl = Cast<AOblivioCharacter>(const_cast<AActor*>(Cursor)))
			{
				return Obl;
			}
			if (APawn* const Pawn = Cast<APawn>(const_cast<AActor*>(Cursor)))
			{
				if (Pawn->IsPlayerControlled())
				{
					return Pawn;
				}
			}
		}
	}

	return nullptr;
}

void AEnemyBase::NotifyEnemyDamageApplied(float AppliedDamage)
{
	if (AppliedDamage > KINDA_SMALL_NUMBER)
	{
		PlayHitBurnNiagaraEffect();
	}
}

void AEnemyBase::PlayHitBurnNiagaraEffect()
{
	if (!bPlayHitBurnNiagaraOnDamage || !IsAlive() || !HitBurnNiagaraComponent)
	{
		return;
	}

	UNiagaraSystem* Fx = HitBurnNiagaraSystem;
	if (!Fx)
	{
		Fx = HitBurnNiagaraComponent->GetAsset();
	}
	if (!Fx)
	{
		return;
	}

	HitBurnNiagaraComponent->SetAsset(Fx);
	HitBurnNiagaraComponent->Activate(true);
}

// 손전등 피격 — LightStunBuildupSeconds 누적 후 경직 발동.
// 파생 클래스(ScreamEnemy, HeadlessLoverEnemy)는 이 메서드를 완전 오버라이드한다.
void AEnemyBase::OnLightHit(float Intensity, float Duration)
{
	if (!IsAlive()) return;

	const float ClampedDuration  = FMath::Max(0.0f, Duration);
	const float ClampedIntensity = FMath::Clamp(Intensity, 0.0f, 1.0f);
	OnEnemyLightHit.Broadcast(this, ClampedIntensity, ClampedDuration);

	// 경직 지속 시간: LightStunDuration > 0 이면 그 값, 아니면 CombatComp->StunDuration 사용
	const float ResolvedStunDur = (LightStunDuration > 0.0f)
		? LightStunDuration
		: (CombatComp ? CombatComp->StunDuration : 1.5f);

	if (LightStunBuildupSeconds <= 0.0f)
	{
		// 즉시 경직
		ApplyCCStun(ResolvedStunDur);
	}
	else
	{
		// 누적 후 임계치 도달 시 경직
		LightExposureAccum += ClampedDuration;
		if (LightExposureAccum >= LightStunBuildupSeconds)
		{
			LightExposureAccum = 0.0f;
			ApplyCCStun(ResolvedStunDur);
		}
	}
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
	if (!IsSoundInvestigationEnabled())
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

bool AEnemyBase::IsAggroDistanceToTargetInsideCylinderIgnoringLos() const
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

bool AEnemyBase::IsAggroCylinderSatisfiedIgnoringLineOfSight() const
{
	return IsAggroDistanceToTargetInsideCylinderIgnoringLos();
}

bool AEnemyBase::IsEncounterAggroGateSatisfiedForBarrier() const
{
	return HasValidAggroTarget() || IsAggroDistanceToTargetInsideCylinderIgnoringLos();
}

bool AEnemyBase::IsAggroDistanceSatisfiedForTarget() const
{
	return IsAggroDistanceToTargetInsideCylinderIgnoringLos();
}

// 유효 폰 + AggroRadius(0이면 거리 무시). 수평 전용이면 Z 무시.
bool AEnemyBase::HasValidAggroTarget() const
{
	return IsAggroDistanceSatisfiedForTarget();
}

EEnemyAIState AEnemyBase::SelectStateWhileAggroed() const
{
	if (const UWorld* World = GetWorld())
	{
		const float Now = World->GetTimeSeconds();
		if (Now < MeleePostAttackForcedChaseEndWorldTime)
		{
			return EEnemyAIState::Chase;
		}
		if (Now < MeleeAttackSwingStateLockEndWorldTime)
		{
			return EEnemyAIState::Attack;
		}
	}

	return IsTargetInAttackRange() ? EEnemyAIState::Attack : EEnemyAIState::Chase;
}

void AEnemyBase::ClearMeleeAttackSwingStateLock()
{
	MeleeAttackSwingStateLockEndWorldTime = -BIG_NUMBER;
	MeleePostAttackForcedChaseEndWorldTime = -BIG_NUMBER;
}

void AEnemyBase::RefreshMeleeAttackSwingStateLock(float WorldTimeSeconds)
{
	const float LockDur = (MeleeAttackStateLockDurationSeconds > KINDA_SMALL_NUMBER)
		? MeleeAttackStateLockDurationSeconds
		: AttackCooldown;
	MeleeAttackSwingStateLockEndWorldTime = WorldTimeSeconds + FMath::Max(0.05f, LockDur);
}

void AEnemyBase::RefreshMeleePostAttackForcedChase(float WorldTimeSeconds)
{
	if (MeleePostAttackForcedChaseDurationSeconds > KINDA_SMALL_NUMBER)
	{
		MeleePostAttackForcedChaseEndWorldTime =
			WorldTimeSeconds + FMath::Max(MeleePostAttackForcedChaseDurationSeconds, KINDA_SMALL_NUMBER);
		return;
	}
	MeleePostAttackForcedChaseEndWorldTime = -BIG_NUMBER;
}

bool AEnemyBase::TryConsumeSpecialFSMUpdate()
{
	return false;
}

void AEnemyBase::ApplyAggroCombatTransientCleanup()
{
	ClearLightTrackState();
	if (IsValid(TargetActor))
	{
		LastKnownTargetLocation = TargetActor->GetActorLocation();
	}
	SearchTimeRemaining = 0.0f;
	SearchRetargetCooldown = 0.0f;
	bHasPendingInvestigate = false;
	bHadAggroLastTick = true;
}

// 어그로 → Chase/Attack, 없으면 TrackLight(옵션) → 방금 놓침 Search → 자극 Investigate → …
void AEnemyBase::UpdateState()
{
	if (!IsAlive())
	{
		SetEnemyState(EEnemyAIState::Dead);
		return;
	}

	if (TryConsumeSpecialFSMUpdate())
	{
		return;
	}

	const bool bAggro = HasValidAggroTarget();

	if (bAggro)
	{
		ApplyAggroCombatTransientCleanup();
		SetEnemyState(SelectStateWhileAggroed());
		return;
	}

	ClearMeleeAttackSwingStateLock();

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

	if (bEnableWandering && !PatrolPoints.IsEmpty())
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

bool AEnemyBase::PassesEnemyAdditionalFlashlightTrackLineOfSight(
	USpotLightComponent const* /*Spot*/, FVector const& /*EnemyLightSampleWorld*/) const
{
	return true;
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
		if (!PassesEnemyAdditionalFlashlightTrackLineOfSight(Spot, TestPoint))
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

	if (!PassesEnemyAdditionalFlashlightTrackLineOfSight(Spot, TestPoint))
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

	const float MaxAcceptanceForAttack = FMath::Max(1.0f, AttackRange - ChaseProximityBuffer);
	const float EffectiveChaseAcceptance = FMath::Min(ChaseAcceptanceRadius, MaxAcceptanceForAttack);

	// 이미 NavMesh 경로를 따라 이동 중이고 타겟이 ChasePathRefreshDistance 이하로만 움직였으면 재요청 생략.
	// 매 프레임 MoveToActor를 호출하면 PathFollowingComponent가 진행 중인 경로를 취소·재시작하여
	// 적이 경로 재계획 중에 멈추는 현상이 생긴다.
	if (const UPathFollowingComponent* PFC = EnemyController->GetPathFollowingComponent())
	{
		if (PFC->GetStatus() == EPathFollowingStatus::Moving)
		{
			const float MovedSq = FVector::DistSquared(TargetActor->GetActorLocation(), LastChaseRequestedTargetPos);
			if (MovedSq < FMath::Square(ChasePathRefreshDistance))
			{
				return;
			}
		}
	}

	const EPathFollowingRequestResult::Type MoveResult = EnemyController->MoveToActor(TargetActor, EffectiveChaseAcceptance);
	if (MoveResult == EPathFollowingRequestResult::Failed)
	{
		// NavMesh로 직접 도달 불가 → 우회 지점 탐색
		UE_LOG(LogTemp, Warning, TEXT("%s: NavMesh path to %s failed — attempting stuck recovery"), *GetNameSafe(this), *GetNameSafe(TargetActor));
		TryRecoverFromStuck();
	}
	else
	{
		LastChaseRequestedTargetPos = TargetActor->GetActorLocation();
	}
}

// Chase 중 주기적으로 '막힘' 여부를 판단한다.
// StuckCheckInterval 마다 현재 위치와 이전 체크 위치를 비교해
// StuckDistanceThreshold 이하로 이동했으면 막힘 카운터를 증가시키고
// StuckCountThreshold 회 연속 시 TryRecoverFromStuck을 호출한다.
void AEnemyBase::CheckAndRecoverFromStuck(float DeltaSeconds)
{
	if (!IsStuckRecoveryEnabled())
	{
		return;
	}

	StuckCheckTimer += DeltaSeconds;
	if (StuckCheckTimer < StuckCheckInterval)
	{
		return;
	}
	StuckCheckTimer = 0.0f;

	const FVector CurrentLocation = GetActorLocation();
	const float MovedDist = FVector::Dist(CurrentLocation, LastStuckCheckLocation);
	LastStuckCheckLocation = CurrentLocation;

	if (MovedDist < StuckDistanceThreshold)
	{
		StuckCounter++;
		if (StuckCounter >= StuckCountThreshold)
		{
			StuckCounter = 0;
			TryRecoverFromStuck();
		}
	}
	else
	{
		StuckCounter = 0;
	}
}

// 적이 벽 등에 막혔을 때 NavMesh 위 우회 지점(좌·우·앞-대각 방향)으로 임시 이동한다.
// 후보 지점이 없으면 수용 반경을 넓혀 원래 타겟으로 재요청(폴백).
void AEnemyBase::TryRecoverFromStuck()
{
	AAIController* EnemyController = Cast<AAIController>(GetController());
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!EnemyController || !NavSys || !IsValid(TargetActor))
	{
		return;
	}

	const FVector MyLocation = GetActorLocation();
	const FVector ToTarget   = (TargetActor->GetActorLocation() - MyLocation).GetSafeNormal();
	const FVector RightDir   = FVector::CrossProduct(ToTarget, FVector::UpVector).GetSafeNormal();
	const FVector QueryExtent(StuckRecoveryRadius * 0.6f, StuckRecoveryRadius * 0.6f, 200.0f);

	// 우선순위: 좌/우 측면 → 앞+우/앞+좌 대각
	const TArray<FVector> Candidates =
	{
		MyLocation + RightDir  * StuckRecoveryRadius,
		MyLocation - RightDir  * StuckRecoveryRadius,
		MyLocation + ToTarget  * (StuckRecoveryRadius * 0.5f) + RightDir * (StuckRecoveryRadius * 0.5f),
		MyLocation + ToTarget  * (StuckRecoveryRadius * 0.5f) - RightDir * (StuckRecoveryRadius * 0.5f),
	};

	for (const FVector& Candidate : Candidates)
	{
		FNavLocation NavLoc;
		if (NavSys->ProjectPointToNavigation(Candidate, NavLoc, QueryExtent))
		{
			EnemyController->MoveToLocation(NavLoc.Location, 40.0f, false);
			// 우회 이동 완료 후 UpdateChase가 타겟 경로를 다시 계획하도록 강제.
			LastChaseRequestedTargetPos = FVector::ZeroVector;
			UE_LOG(LogTemp, Warning, TEXT("%s: stuck recovery → offset NavMesh point"), *GetNameSafe(this));
			return;
		}
	}

	// 우회 후보 없음(좁은 공간 등): 수용 반경 2배로 직접 재요청
	const float MaxAcceptance   = FMath::Max(1.0f, AttackRange - ChaseProximityBuffer);
	const float WiderAcceptance = FMath::Min(ChaseAcceptanceRadius * 2.0f, MaxAcceptance);
	EnemyController->MoveToActor(TargetActor, WiderAcceptance);
	LastChaseRequestedTargetPos = FVector::ZeroVector;
	UE_LOG(LogTemp, Warning, TEXT("%s: stuck recovery fallback — wider acceptance re-request"), *GetNameSafe(this));
}

// =============================================================================
// HandleBlockingObstacle
// 플레이어 추격 중 시야가 크래프팅 장애물에 의해 막히면 CraftingLosBlockedEvalCount 를 누적하고,
// Threshold(기본 10)회 스캔마다 장애물 파괴 모드로 전환한다. 그 전에는 추격(UpdateChase)만 수행.
// 장애물 파괴는 근접 한 대당 고정 1 HP(박스 Max 5 / 벽 7 / 바리케이트 10).
// =============================================================================

bool AEnemyBase::LineTraceLosBlockedByCraftingObstacle(AActor* const Target, FHitResult& OutHit) const
{
	OutHit = FHitResult();
	if (!IsValid(Target) || !GetWorld())
	{
		return false;
	}

	const FVector TraceStart = GetActorLocation() + FVector(0.f, 0.f, 50.f);
	FVector TraceEnd = Target->GetActorLocation();
	if (APawn const* P = Cast<APawn>(Target))
	{
		if (UCapsuleComponent const* Caps = P->FindComponentByClass<UCapsuleComponent>())
		{
			TraceEnd = Caps->GetComponentLocation();
		}
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(EnemyCraftingLosScan), false, this);
	Params.AddIgnoredActor(Target);

	if (!GetWorld()->LineTraceSingleByChannel(OutHit, TraceStart, TraceEnd, ECC_Visibility, Params))
	{
		return false;
	}

	return Cast<AObstacleBase>(OutHit.GetActor()) != nullptr;
}

void AEnemyBase::HandleBlockingObstacle(float DeltaSeconds)
{
	if (!IsObstacleAttackEnabled())
	{
		return;
	}

	if (!IsValid(TargetActor))
	{
		BlockingObstacle = nullptr;
		CraftingLosBlockedEvalCount = 0;
		bPrioritizeBreakingCraftingObstacle = false;
		return;
	}

	constexpr float CraftingMeleeSwingDamage = 1.f;

	ObstacleScanTimer += DeltaSeconds;
	const bool bDoLosEval = ObstacleScanTimer >= ObstacleScanInterval;
	if (bDoLosEval)
	{
		ObstacleScanTimer = 0.f;
	}

	FHitResult LosHit;
	const bool bLosBlockedByCrafting = LineTraceLosBlockedByCraftingObstacle(TargetActor, LosHit);
	AObstacleBase* const LosObs = bLosBlockedByCrafting ? Cast<AObstacleBase>(LosHit.GetActor()) : nullptr;

	if (bDoLosEval)
	{
		if (LosObs)
		{
			++CraftingLosBlockedEvalCount;
			if (CraftingLosBlockedEvalCount >= CraftingLosBlockedEvaluationThreshold)
			{
				bPrioritizeBreakingCraftingObstacle = true;
			}
		}
		else
		{
			CraftingLosBlockedEvalCount = 0;
			bPrioritizeBreakingCraftingObstacle = false;
		}
	}

	const float DistSqPlayer = FVector::DistSquared(GetActorLocation(), TargetActor->GetActorLocation());
	const bool bPlayerInMeleeRange = DistSqPlayer <= FMath::Square(AttackRange);

	if (bPlayerInMeleeRange && !bPrioritizeBreakingCraftingObstacle)
	{
		BlockingObstacle = nullptr;
		return;
	}

	if (!bPrioritizeBreakingCraftingObstacle)
	{
		BlockingObstacle = nullptr;
		return;
	}

	AObstacleBase* Obs = LosObs;
	if (!IsValid(Obs))
	{
		Obs = Cast<AObstacleBase>(BlockingObstacle);
	}
	else
	{
		BlockingObstacle = LosObs;
	}

	if (!IsValid(Obs))
	{
		BlockingObstacle = nullptr;
		return;
	}

	BlockingObstacle = Obs;

	const float DistToObs = FVector::Dist(GetActorLocation(), Obs->GetActorLocation());

	if (DistToObs <= AttackRange)
	{
		ObstacleAttackTimer += DeltaSeconds;
		if (ObstacleAttackTimer >= AttackCooldown)
		{
			ObstacleAttackTimer = 0.f;
			UGameplayStatics::ApplyDamage(Obs, CraftingMeleeSwingDamage, GetController(), this,
				UDamageType::StaticClass());
			UE_LOG(LogTemp, Log, TEXT("%s attacking crafting obstacle %s (%.1f hp left)"),
				*GetNameSafe(this), *GetNameSafe(Obs), Obs->GetCurrentHealth());
		}
		StopEnemyMovement();
	}
	else
	{
		if (AAIController* AC = Cast<AAIController>(GetController()))
		{
			AC->MoveToActor(Obs, FMath::Min(AttackRange * 0.8f, 80.f));
		}
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
	RefreshMeleeAttackSwingStateLock(CurrentTime);
	RefreshMeleePostAttackForcedChase(CurrentTime);
	PerformAttack(TargetActor);
}

// 기본 근접: 범위 내 공격 가능 판단만 브로드캐스트. 실제 공격 방식은 전투 시스템/BP에서 담당.
void AEnemyBase::PerformAttack_Implementation(AActor* Target)
{
	(void)Target;
	// 근접 타격은 UEnemyMeleeCommitNotify → CommitAttackFromAnimNotify 에서 처리. BP/C++ 전용 공격은 PerformAttack 을 오버라이드.
}

void AEnemyBase::CommitAttackFromAnimNotify(AActor* OptionalTargetOverride)
{
	if (!IsAlive())
	{
		return;
	}

	AActor* const HitTarget = IsValid(OptionalTargetOverride) ? OptionalTargetOverride : TargetActor.Get();
	if (!IsValid(HitTarget))
	{
		return;
	}

	if (!IsMeleeCommitNotifyHitValid(HitTarget))
	{
		return;
	}

	DispatchEnemyAttackCommitted(HitTarget);
}

bool AEnemyBase::IsMeleeCommitNotifyHitValid(AActor const* HitTarget) const
{
	if (!IsValid(HitTarget))
	{
		return false;
	}
	const float MaxReach = FMath::Max(0.f, AttackRange + MeleeCommitRangeSlackCm);
	return FVector::DistSquared(GetActorLocation(), HitTarget->GetActorLocation()) <= FMath::Square(MaxReach);
}

void AEnemyBase::DispatchEnemyAttackCommitted(AActor* Target)
{
	DispatchEnemyAttackCommitted(Target, AttackDamage, nullptr);
}

void AEnemyBase::DispatchEnemyAttackCommitted(AActor* Target, float DamageAmount)
{
	DispatchEnemyAttackCommitted(Target, DamageAmount, nullptr);
}

void AEnemyBase::DispatchEnemyAttackCommitted(AActor* Target, float DamageAmount, TSubclassOf<UDamageType> DamageTypeClass)
{
	if (!IsValid(Target))
	{
		return;
	}

	UClass* const TypePtr = DamageTypeClass.Get();
	OnEnemyAttackCommitted.Broadcast(this, Target, DamageAmount, TypePtr);
	if (UWorld* World = GetWorld())
	{
		if (UEnemyCombatRegistrySubsystem* Reg = World->GetSubsystem<UEnemyCombatRegistrySubsystem>())
		{
			Reg->NotifyEnemyAttackCommitted(this, Target, DamageAmount, TypePtr);
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

	if (!bEnableWandering)
	{
		StopEnemyMovement();
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
	// 어그로가 생긴 채로 Idle에 남아 있는 경우(AggroRadius=0 등) 즉시 Chase로 전환
	if (HasValidAggroTarget())
	{
		SetEnemyState(SelectStateWhileAggroed());
		return;
	}

	if (!bEnableWandering || !bEnableIdleWander)
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
	ClearMeleeAttackSwingStateLock();
	CurrentHealth = 0.0f;
	SetEnemyState(EEnemyAIState::Dead);
	StopEnemyMovement();

	if (MeleeAttackRangeIndicatorMesh)
	{
		MeleeAttackRangeIndicatorMesh->SetVisibility(false);
	}

	if (HitBurnNiagaraComponent)
	{
		HitBurnNiagaraComponent->DeactivateImmediate();
	}

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

// 동일 상태면 무시(bForce 시 이속·Chase 초기화 등 갱신 — 심작 종료 후 추격 재개 등).
void AEnemyBase::SetEnemyState(EEnemyAIState NewState, bool bForce)
{
	if (!bForce && EnemyState == NewState)
	{
		return;
	}

	const EEnemyAIState OldState = EnemyState;
	EnemyState = NewState;

	// Chase/Attack/TrackLight 진입 시 ChaseMoveSpeed를, Idle/Investigate/Search 등에서는 MoveSpeed를 반영한다.
	// BP에 낮은 MoveSpeed가 저장된 적이 Chase 상태로 바뀌어도 이전 MaxWalkSpeed에 묶여 멈추는 문제를 방지.
	RefreshWalkSpeedFromSources();

	if (NewState == EEnemyAIState::Idle && bEnableWandering && bEnableIdleWander)
	{
		IdleWanderRetargetCooldown = FMath::FRandRange(0.3f, 1.5f);
	}
	if (NewState == EEnemyAIState::Chase)
	{
		// Chase 진입 시 막힘 감지 상태 초기화 — 직전 경로 이력이 오탐을 유발하지 않도록.
		LastStuckCheckLocation = GetActorLocation();
		LastChaseRequestedTargetPos = FVector::ZeroVector;
		StuckCheckTimer = 0.0f;
		StuckCounter = 0;
	}
	// Chase 이외 상태로 전환되면 장애물 타겟 해제
	if (NewState != EEnemyAIState::Chase)
	{
		BlockingObstacle = nullptr;
		ObstacleScanTimer = 0.0f;
		ObstacleAttackTimer = 0.0f;
		CraftingLosBlockedEvalCount = 0;
		bPrioritizeBreakingCraftingObstacle = false;
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

void AEnemyBase::NotifyEnemyStateChanged(EEnemyAIState OldState, EEnemyAIState NewState)
{
	(void)OldState;
	(void)NewState;
	/** Tank 등: 내부 EnemyState 값과 브루 노출 FSM 이 다를 수 있어 GetEnemyState 기준으로 앰비언트 갱신. */
	UpdateIdleChaseLocomotionAmbientForFsmState(GetEnemyState());
}

AEnemyBase::ELocomotionAmbientLayer AEnemyBase::LocomotionAmbientLayerFromFsmState(EEnemyAIState State)
{
	switch (State)
	{
	case EEnemyAIState::Idle:
		return ELocomotionAmbientLayer::Idle;
	case EEnemyAIState::JumpAttack:
		return ELocomotionAmbientLayer::None;
	case EEnemyAIState::Chase:
	case EEnemyAIState::Attack:
	case EEnemyAIState::Heartbeat:
	case EEnemyAIState::Membrane:
	case EEnemyAIState::PlacentaDefense:
		return ELocomotionAmbientLayer::Chase;
	default:
		return ELocomotionAmbientLayer::None;
	}
}

void AEnemyBase::SyncIdleChaseLocomotionAmbientToFsm(EEnemyAIState ResolvedFsmState)
{
	UpdateIdleChaseLocomotionAmbientForFsmState(ResolvedFsmState);
}

void AEnemyBase::RefreshIdleChaseLocomotionAmbientFromCurrentDisplayState()
{
	UpdateIdleChaseLocomotionAmbientForFsmState(GetEnemyState());
}

USoundBase* AEnemyBase::GetLocomotionAmbientSoundForLayer(ELocomotionAmbientLayer Layer) const
{
	switch (Layer)
	{
	case ELocomotionAmbientLayer::Idle:
		return IdleLocomotionAmbientSound.Get();
	case ELocomotionAmbientLayer::Chase:
		return ChaseLocomotionAmbientSound.Get();
	default:
		return nullptr;
	}
}

void AEnemyBase::ApplyIdleChaseLocomotionAmbientVolume()
{
	if (!IdleChaseLocomotionAudioComponent)
	{
		return;
	}

	const float CombinedVol = FMath::Max(0.0f, EnemySoundVolumeMultiplier);
	IdleChaseLocomotionAudioComponent->SetVolumeMultiplier(CombinedVol);
}

void AEnemyBase::StopIdleChaseLocomotionAmbientImmediate()
{
	if (!IdleChaseLocomotionAudioComponent)
	{
		return;
	}

	const bool bPrevSuppress = bIdleChaseLocomotionAmbientSuppressFinished;
	bIdleChaseLocomotionAmbientSuppressFinished = true;
	IdleChaseLocomotionAudioComponent->Stop();
	CurrentLocomotionAmbientLayer = ELocomotionAmbientLayer::None;
	bIdleChaseLocomotionAmbientSuppressFinished = bPrevSuppress;
}

void AEnemyBase::StartIdleChaseLocomotionAmbientPlay(ELocomotionAmbientLayer Layer, USoundBase* Sound)
{
	if (!IdleChaseLocomotionAudioComponent || !Sound)
	{
		return;
	}

	bIdleChaseLocomotionAmbientSuppressFinished = true;
	IdleChaseLocomotionAudioComponent->Stop();
	IdleChaseLocomotionAudioComponent->SetSound(Sound);
	ApplyIdleChaseLocomotionAmbientVolume();
	CurrentLocomotionAmbientLayer = Layer;
	bIdleChaseLocomotionAmbientSuppressFinished = false;

	IdleChaseLocomotionAudioComponent->FadeIn(
		FMath::Max(0.02f, IdleChaseLocomotionAmbientFadeInDuration),
		1.f,
		0.f,
		EAudioFaderCurve::Linear);
}

void AEnemyBase::UpdateIdleChaseLocomotionAmbientForFsmState(EEnemyAIState NewState)
{
	if (!IdleChaseLocomotionAudioComponent || !GetWorld())
	{
		return;
	}

	UWorld* const World = GetWorld();
	const ELocomotionAmbientLayer DesiredLayer = LocomotionAmbientLayerFromFsmState(NewState);
	USoundBase* const DesiredSound = GetLocomotionAmbientSoundForLayer(DesiredLayer);

	const auto IsPlaybackActiveOrFading = [](EAudioComponentPlayState PS) -> bool {
		return PS == EAudioComponentPlayState::Playing || PS == EAudioComponentPlayState::FadingIn
			|| PS == EAudioComponentPlayState::FadingOut;
	};

	const EAudioComponentPlayState CurrentPlayState = IdleChaseLocomotionAudioComponent->GetPlayState();
	const bool bAudioBusy = IsPlaybackActiveOrFading(CurrentPlayState);

	const bool bWantSilence = (DesiredLayer == ELocomotionAmbientLayer::None || DesiredSound == nullptr);

	if (bWantSilence)
	{
		World->GetTimerManager().ClearTimer(IdleChaseLocomotionAmbientFadeTimerHandle);

		if (!bAudioBusy && CurrentLocomotionAmbientLayer == ELocomotionAmbientLayer::None)
		{
			return;
		}

		bIdleChaseLocomotionAmbientSuppressFinished = true;

		if (bAudioBusy)
		{
			const float Fo = FMath::Max(0.05f, IdleChaseLocomotionAmbientFadeOutDuration);
			IdleChaseLocomotionAudioComponent->FadeOut(Fo, 0.f, EAudioFaderCurve::Linear);
			World->GetTimerManager().SetTimer(
				IdleChaseLocomotionAmbientFadeTimerHandle,
				this,
				&AEnemyBase::OnIdleChaseLocomotionAmbientFadeFinished,
				Fo,
				false);
		}
		else
		{
			StopIdleChaseLocomotionAmbientImmediate();
			bIdleChaseLocomotionAmbientSuppressFinished = false;
		}

		return;
	}

	if (DesiredLayer == CurrentLocomotionAmbientLayer && IdleChaseLocomotionAudioComponent->GetSound() == DesiredSound)
	{
		if (IsPlaybackActiveOrFading(CurrentPlayState))
		{
			return;
		}
	}

	World->GetTimerManager().ClearTimer(IdleChaseLocomotionAmbientFadeTimerHandle);

	bIdleChaseLocomotionAmbientSuppressFinished = true;

	if (bAudioBusy)
	{
		const float Fo = FMath::Max(0.05f, IdleChaseLocomotionAmbientFadeOutDuration);
		IdleChaseLocomotionAudioComponent->FadeOut(Fo, 0.f, EAudioFaderCurve::Linear);
		World->GetTimerManager().SetTimer(
			IdleChaseLocomotionAmbientFadeTimerHandle,
			this,
			&AEnemyBase::OnIdleChaseLocomotionAmbientFadeFinished,
			Fo,
			false);
	}
	else
	{
		bIdleChaseLocomotionAmbientSuppressFinished = false;
		StartIdleChaseLocomotionAmbientPlay(DesiredLayer, DesiredSound);
	}
}

void AEnemyBase::OnIdleChaseLocomotionAmbientFadeFinished()
{
	bIdleChaseLocomotionAmbientSuppressFinished = false;

	if (!IdleChaseLocomotionAudioComponent)
	{
		return;
	}

	const ELocomotionAmbientLayer DesiredLayer = LocomotionAmbientLayerFromFsmState(GetEnemyState());
	USoundBase* const DesiredSound = GetLocomotionAmbientSoundForLayer(DesiredLayer);

	if (DesiredLayer == ELocomotionAmbientLayer::None || DesiredSound == nullptr)
	{
		bIdleChaseLocomotionAmbientSuppressFinished = true;
		IdleChaseLocomotionAudioComponent->Stop();
		CurrentLocomotionAmbientLayer = ELocomotionAmbientLayer::None;
		bIdleChaseLocomotionAmbientSuppressFinished = false;
		return;
	}

	StartIdleChaseLocomotionAmbientPlay(DesiredLayer, DesiredSound);
}

void AEnemyBase::OnIdleChaseLocomotionAmbientPlaybackFinished()
{
	if (bIdleChaseLocomotionAmbientSuppressFinished || !IdleChaseLocomotionAudioComponent)
	{
		return;
	}

	const ELocomotionAmbientLayer WantLayer = LocomotionAmbientLayerFromFsmState(GetEnemyState());
	USoundBase* const ExpectedSound = GetLocomotionAmbientSoundForLayer(WantLayer);

	if (WantLayer == ELocomotionAmbientLayer::None || ExpectedSound == nullptr)
	{
		return;
	}

	if (WantLayer != CurrentLocomotionAmbientLayer)
	{
		return;
	}

	if (IdleChaseLocomotionAudioComponent->GetSound() != ExpectedSound)
	{
		return;
	}

	ApplyIdleChaseLocomotionAmbientVolume();
	IdleChaseLocomotionAudioComponent->Play();
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

	// 탱커 심작·Luxeater 차징 등: Chase 이속이 남으면 ABP 속도 블렌드가 심작/차징 위에 덮임
	if (ShouldSuppressAILocomotion())
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
	const EEnemyAIState LocState = GetEnemyState();
	const bool bCombatLocomotion =
		LocState == EEnemyAIState::Chase ||
		LocState == EEnemyAIState::Attack ||
		LocState == EEnemyAIState::Heartbeat ||
		LocState == EEnemyAIState::JumpAttack ||
		LocState == EEnemyAIState::Membrane ||
		LocState == EEnemyAIState::PlacentaDefense ||
		LocState == EEnemyAIState::TrackLight;
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

void AEnemyBase::DrawDebugCombatExtras()
{
}

void AEnemyBase::UpdateMeleeAttackRangeIndicatorVisual()
{
	if (!MeleeAttackRangeIndicatorMesh)
	{
		return;
	}

	const bool bShouldShow =
		ShouldShowMeleeAttackRangeIndicator() && IsAlive() && IsValid(MeleeAttackRangeIndicatorMaterial);

	const float RadiusCm = GetMeleeAttackRangeIndicatorRadiusCm();

	if (!bShouldShow || RadiusCm <= KINDA_SMALL_NUMBER || MeleeAttackRangeIndicatorBuiltInSphereRadiusUU <= KINDA_SMALL_NUMBER)
	{
		MeleeAttackRangeIndicatorMesh->SetVisibility(false);
		MeleeAttackRangeIndicatorMesh->SetHiddenInGame(true);
		return;
	}

	if (const UCapsuleComponent* Cap = GetCapsuleComponent())
	{
		MeleeAttackRangeIndicatorMesh->SetRelativeLocation(FVector(0.f, 0.f, -Cap->GetScaledCapsuleHalfHeight()));
	}

	const float XYScale = RadiusCm / MeleeAttackRangeIndicatorBuiltInSphereRadiusUU;
	const float ZScale = FMath::Clamp(MeleeAttackRangeIndicatorDiskThicknessScale, 0.001f, 2.f);
	MeleeAttackRangeIndicatorMesh->SetRelativeScale3D(FVector(XYScale, XYScale, ZScale));
	MeleeAttackRangeIndicatorMesh->SetHiddenInGame(false);
	MeleeAttackRangeIndicatorMesh->SetVisibility(true);
}

void AEnemyBase::SetEnemySoundVolumeMultiplier(float NewMultiplier)
{
	EnemySoundVolumeMultiplier = FMath::Clamp(NewMultiplier, 0.0f, 4.0f);
	ApplyEnemySoundVolumes();
}

void AEnemyBase::ApplyEnemySoundVolumes()
{
	const EAudioComponentPlayState PS =
		IdleChaseLocomotionAudioComponent ? IdleChaseLocomotionAudioComponent->GetPlayState() : EAudioComponentPlayState::Stopped;
	if (PS == EAudioComponentPlayState::Playing || PS == EAudioComponentPlayState::FadingIn
		|| PS == EAudioComponentPlayState::FadingOut)
	{
		ApplyIdleChaseLocomotionAmbientVolume();
	}
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