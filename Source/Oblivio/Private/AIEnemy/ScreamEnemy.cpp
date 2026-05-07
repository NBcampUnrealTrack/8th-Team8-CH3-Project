#include "AIEnemy/ScreamEnemy.h"

#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"

// =============================================================================
// AScreamEnemy 구현 요약
// Tick: 페이즈에 따라 분기 — Ready/Cooldown 은 베이스 FSM 작동, 능력 활성 페이즈
//       (Charging/Transit/Root) 동안에는 베이스 이동을 우리 오버라이드로 봉인.
// 능력 사이클: Ready → Charging → Transit → (PerformAttack) → Root → Cooldown → Ready
// =============================================================================

AScreamEnemy::AScreamEnemy()
{
	PrimaryActorTick.bCanEverTick = true;

	MaxHealth = 110.0f;
	CurrentHealth = MaxHealth;
	MoveSpeed = 220.0f;
	ChaseMoveSpeed = 280.0f;
	AttackDamage = 0.0f;
	AttackRange = 200.0f;
	AttackCooldown = 1.0f;
	ChaseAcceptanceRadius = 60.0f;
	ChaseProximityBuffer = 40.0f;

	// 비전투 시 천천히 배회. 0이면 EnemyBase 규칙상 무한 어그로이므로 Scream 기본값은 유한 반경.
	AggroRadius = 1800.0f;
	bAggroUseHorizontalDistance = true;
	bEnableIdleWander = true;
	bEnableLightTracking = false;
}

void AScreamEnemy::BeginPlay()
{
	// 기존 BP가 0(EnemyBase 기준 무한 어그로)으로 저장돼 있어도 Scream은 기본적으로 유한 어그로를 강제한다.
	if (AggroRadius <= 0.0f)
	{
		AggroRadius = 1800.0f;
	}
	// 기존 BP에 잘못 저장된 값이 있어도 기획 고정값(3초 누적 -> 3초 경직 -> 30초 면역)을 보장한다.
	LightStunBuildupSeconds = 3.0f;
	LightStunDuration = 3.0f;
	LightStunImmunitySeconds = 30.0f;

	Super::BeginPlay();

	SpawnZ = GetActorLocation().Z;

	if (const UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		CachedMovementMode = static_cast<uint8>(Move->MovementMode);
		CachedMaxFlySpeed = Move->MaxFlySpeed;
	}
	if (const UCapsuleComponent* Cap = GetCapsuleComponent())
	{
		CachedWorldStaticResponse = Cap->GetCollisionResponseToChannel(ECC_WorldStatic);
	}
	if (const USkeletalMeshComponent* MeshComp = GetMesh())
	{
		CachedMeshWorldStaticResponse = MeshComp->GetCollisionResponseToChannel(ECC_WorldStatic);
	}

	AbilityPhase = EScreamAbilityPhase::Ready;
	PhaseElapsed = 0.0f;
	CooldownRemaining = 0.0f;
	ReadyArmDelayRemaining = 0.0f;
	bHasGhostWanderGoal = false;

	BlinksRemainingInCycle = 0;
	bIsPhaseTwo = false;
	LightExposureAccum = 0.0f;
	LightStunUntilSec = 0.0;
	LightImmuneUntilSec = 0.0;
	if (UWorld* W = GetWorld())
	{
		LastLightHitWorldTime = W->GetTimeSeconds();
	}
	UpdateHealthPhase();

	// 시작 시 즉시 차지하지 않고 한 번의 쿨다운(AbilityCooldownSeconds, 기본 90초)부터 진입.
	// 의도: 첫 사이클은 wall-ignore wander 로 압박만 주고, 이후 첫 블링크가 발동.
	if (bStartOnCooldown)
	{
		StartCooldown();
	}
}

void AScreamEnemy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bGhostActive)
	{
		EnableGhostMovement(false);
	}
	Super::EndPlay(EndPlayReason);
}

void AScreamEnemy::Die()
{
	if (bGhostActive)
	{
		EnableGhostMovement(false);
	}

	LightExposureAccum = 0.0f;
	LightStunUntilSec = 0.0;
	LightImmuneUntilSec = 0.0;
	BlinksRemainingInCycle = 0;

	Super::Die();
}

void AScreamEnemy::Tick(float DeltaSeconds)
{
	if (!IsAlive())
	{
		Super::Tick(DeltaSeconds);
		return;
	}

	if (!IsValid(TargetActor))
	{
		FindDefaultTarget();
	}

	// 1) 광원 누적/면역 윈도 정리. 누적 임계 도달 시 라이트 스턴이 트리거된다.
	UpdateLightStunState(DeltaSeconds);

	// 2) 라이트 스턴 활성: 베이스 FSM(이미 bCCStunned 로 게이트됨)만 돌리고 능력 진행은 멈춘다.
	if (IsLightStunActive())
	{
		Super::Tick(DeltaSeconds);
		StopEnemyMovement();
		return;
	}

	PhaseElapsed += DeltaSeconds;

	switch (AbilityPhase)
	{
	case EScreamAbilityPhase::Ready:
		// 베이스 FSM 이 일반 추격/배회를 운영한다. 그 뒤 차지 평가.
		Super::Tick(DeltaSeconds);
		TryStartChargeIfReady();
		break;

	case EScreamAbilityPhase::Charging:
		// 베이스 FSM 의 상태 전이는 살리되, UpdateChase/Attack/Idle 오버라이드에서 이동을 봉인한다.
		Super::Tick(DeltaSeconds);
		TickCharge(DeltaSeconds);
		break;

	case EScreamAbilityPhase::Transit:
		// 베이스 Tick 을 호출하지 않는다 — 우리가 SetActorLocation 으로 직접 보간하므로
		// 베이스 FSM 이 MoveTo 등으로 끼어들면 흔들림이 생긴다.
		TickTransit(DeltaSeconds);
		break;

	case EScreamAbilityPhase::ChainPause:
		// P2: 1차 블링크 후 짧은 정지. 베이스 이동은 봉인 상태(IsAbilityActive 분기).
		Super::Tick(DeltaSeconds);
		TickChainPause(DeltaSeconds);
		break;

	case EScreamAbilityPhase::Root:
		Super::Tick(DeltaSeconds);
		TickRoot(DeltaSeconds);
		break;

	case EScreamAbilityPhase::Cooldown:
		Super::Tick(DeltaSeconds);
		TickCooldown(DeltaSeconds);
		break;
	}

#if !UE_BUILD_SHIPPING
	if (bDebugDrawAbility && GetWorld())
	{
		const FColor Color =
			AbilityPhase == EScreamAbilityPhase::Charging ? FColor::Yellow :
			AbilityPhase == EScreamAbilityPhase::Transit ? FColor::Red :
			AbilityPhase == EScreamAbilityPhase::Root ? FColor::Cyan :
			AbilityPhase == EScreamAbilityPhase::Cooldown ? FColor::Silver : FColor::Green;

		if (AbilityPhase == EScreamAbilityPhase::Charging || AbilityPhase == EScreamAbilityPhase::Transit)
		{
			DrawDebugSphere(GetWorld(), SnapshotLocation, 60.f, 16, Color, false, 0.f, 0, 2.f);
			DrawDebugLine(GetWorld(), GetActorLocation(), SnapshotLocation, Color, false, 0.f, 0, 1.5f);
		}
		if (GEngine)
		{
			static const TCHAR* PhaseNames[] = { TEXT("Ready"), TEXT("Charging"), TEXT("Transit"), TEXT("ChainPause"), TEXT("Root"), TEXT("Cooldown") };
			const double Now = GetWorld()->GetTimeSeconds();
			const float StunRemain = (float)FMath::Max(0.0, LightStunUntilSec - Now);
			const float ImmuneRemain = (float)FMath::Max(0.0, LightImmuneUntilSec - Now);
			GEngine->AddOnScreenDebugMessage((uint64)(UPTRINT)this, 0.f, Color,
				FString::Printf(TEXT("Scream %s: phase=%s elapsed=%.2f cd=%.1f | P2=%d blinks=%d | lightAcc=%.2f stun=%.1f imm=%.1f"),
					*GetNameSafe(this),
					PhaseNames[(int32)AbilityPhase],
					PhaseElapsed,
					CooldownRemaining,
					bIsPhaseTwo ? 1 : 0,
					BlinksRemainingInCycle,
					LightExposureAccum,
					StunRemain,
					ImmuneRemain));
		}
	}
#endif
}

bool AScreamEnemy::TryStartChargeIfReady()
{
	if (AbilityPhase != EScreamAbilityPhase::Ready)
	{
		return false;
	}
	if (!IsValid(TargetActor))
	{
		return false;
	}
	if (!HasValidAggroTarget())
	{
		return false;
	}

	const float Dist2 = FVector::DistSquared(GetActorLocation(), TargetActor->GetActorLocation());
	if (Dist2 < FMath::Square(MinChargeStartDistance))
	{
		return false;
	}
	if (Dist2 > FMath::Square(MaxChargeStartDistance))
	{
		return false;
	}

	StartCharge();
	return true;
}

void AScreamEnemy::StartCharge()
{
	if (!IsValid(TargetActor))
	{
		return;
	}

	SnapshotLocation = TargetActor->GetActorLocation();
	OnSnapshotTaken.Broadcast(this, SnapshotLocation);

	// 사이클 시작 시 블링크 카운트 결정 — 페이즈 2면 더블 블링크.
	BlinksRemainingInCycle = bIsPhaseTwo ? FMath::Max(1, PhaseTwoBlinksPerCycle) : 1;

	StopEnemyMovement();
	TransitionToPhase(EScreamAbilityPhase::Charging);
}

void AScreamEnemy::TickCharge(float DeltaSeconds)
{
	// 차지 동안 제자리에서 텔레그래프 — 잔여 이동 입력 정리 + 타겟 정렬.
	StopEnemyMovement();
	FaceTarget(DeltaSeconds, ChargeFaceTargetRotateRate);

	if (PhaseElapsed >= ChargeSeconds)
	{
		StartTransit();
	}
}

void AScreamEnemy::StartTransit()
{
	TransitStart = GetActorLocation();

	// Transit 동안만 벽 무시.
	EnableGhostMovement(true);
	StopEnemyMovement();

	TransitionToPhase(EScreamAbilityPhase::Transit);

	if (TransitSeconds <= KINDA_SMALL_NUMBER)
	{
		// 안전장치 — 0초이면 한 번에 도착. 기획상 비권장.
		SetActorLocation(SnapshotLocation, false, nullptr, ETeleportType::TeleportPhysics);
		FinishTransitAndAttack();
	}
}

void AScreamEnemy::TickTransit(float DeltaSeconds)
{
	if (TransitSeconds <= KINDA_SMALL_NUMBER)
	{
		FinishTransitAndAttack();
		return;
	}

	const float Alpha = FMath::Clamp(PhaseElapsed / TransitSeconds, 0.0f, 1.0f);
	const float Eased = FMath::SmoothStep(0.0f, 1.0f, Alpha);
	const FVector NewLoc = FMath::Lerp(TransitStart, SnapshotLocation, Eased);
	SetActorLocation(NewLoc, false, nullptr, ETeleportType::TeleportPhysics);

	if (Alpha >= 1.0f)
	{
		FinishTransitAndAttack();
	}
}

void AScreamEnemy::FinishTransitAndAttack()
{
	// 정확한 스냅샷 좌표에 정렬 후 일반 콜리전 복구.
	SetActorLocation(SnapshotLocation, false, nullptr, ETeleportType::TeleportPhysics);
	EnableGhostMovement(false);

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->Velocity = FVector::ZeroVector;
	}

	// 페이즈 2 더블 블링크: 한 번 더 남았다면 짧은 정지 후 재스냅샷 + 2차 Transit.
	if (BlinksRemainingInCycle > 1)
	{
		--BlinksRemainingInCycle;
		StartChainPause();
		return;
	}

	BlinksRemainingInCycle = 0;

	if (IsValid(TargetActor))
	{
		// 전투 시스템에 공격 의도 위임 — 실 데미지/연출은 OnEnemyAttackCommitted 구독자 몫.
		PerformAttack(TargetActor);
	}

	StartRoot();
}

void AScreamEnemy::StartChainPause()
{
	StopEnemyMovement();
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->Velocity = FVector::ZeroVector;
	}
	TransitionToPhase(EScreamAbilityPhase::ChainPause);
}

void AScreamEnemy::TickChainPause(float DeltaSeconds)
{
	StopEnemyMovement();
	FaceTarget(DeltaSeconds, ChargeFaceTargetRotateRate);

	if (PhaseElapsed >= ChainPauseSeconds)
	{
		// 2차 블링크는 현재 플레이어 위치를 재스냅샷.
		if (IsValid(TargetActor))
		{
			SnapshotLocation = TargetActor->GetActorLocation();
			OnSnapshotTaken.Broadcast(this, SnapshotLocation);
		}
		StartTransit();
	}
}

void AScreamEnemy::StartRoot()
{
	StopEnemyMovement();
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->Velocity = FVector::ZeroVector;
	}
	TransitionToPhase(EScreamAbilityPhase::Root);
}

void AScreamEnemy::TickRoot(float DeltaSeconds)
{
	// 매 틱 잔여 이동/추격 입력 차단 — 베이스 FSM 이 Chase 로 전이해도 UpdateChase 오버라이드가 막아준다.
	StopEnemyMovement();

	if (PhaseElapsed >= PostBlinkRootSeconds)
	{
		StartCooldown();
	}
}

void AScreamEnemy::StartCooldown()
{
	CooldownRemaining = AbilityCooldownSeconds;
	ReadyArmDelayRemaining = ReadyArmDelay;
	bHasGhostWanderGoal = false;
	GhostWanderRetargetCooldown = 0.0f;
	BlinksRemainingInCycle = 0;

	if (bGhostMovementDuringCooldown)
	{
		EnableGhostMovement(true);
	}

	TransitionToPhase(EScreamAbilityPhase::Cooldown);
}

void AScreamEnemy::TickCooldown(float DeltaSeconds)
{
	CooldownRemaining = FMath::Max(0.0f, CooldownRemaining - DeltaSeconds);
	if (ReadyArmDelayRemaining > 0.0f)
	{
		ReadyArmDelayRemaining = FMath::Max(0.0f, ReadyArmDelayRemaining - DeltaSeconds);
	}

	// Ghost 모드일 때만 우리가 직접 비행 배회를 운영한다.
	// (베이스의 MoveTo 는 NavMesh 경로를 따르므로 Flying + 콜리전 무시 와 어울리지 않음.
	//  UpdateChase/Idle 오버라이드는 Ghost 동안 베이스 이동을 비워두고 여기서만 입력을 준다.)
	if (bGhostActive)
	{
		TickGhostWander(DeltaSeconds);
	}

	if (CooldownRemaining <= 0.0f && ReadyArmDelayRemaining <= 0.0f)
	{
		EndCooldown();
	}
}

void AScreamEnemy::EndCooldown()
{
	CooldownRemaining = 0.0f;
	ReadyArmDelayRemaining = 0.0f;
	if (bGhostActive)
	{
		EnableGhostMovement(false);
	}
	TransitionToPhase(EScreamAbilityPhase::Ready);
}

void AScreamEnemy::TickGhostWander(float DeltaSeconds)
{
	GhostWanderRetargetCooldown -= DeltaSeconds;

	// 1) 타겟이 가까우면 천천히 압박(벽 무시 부유 추적).
	if (IsValid(TargetActor) && HasValidAggroTarget())
	{
		const FVector MyLoc = GetActorLocation();
		const FVector TLoc = TargetActor->GetActorLocation();
		const float Dist = FVector::Dist(MyLoc, TLoc);
		if (Dist < MaxChargeStartDistance)
		{
			FVector Dir = (TLoc - MyLoc).GetSafeNormal();
			// Z 클램프: 스폰 Z 기준 ±GhostVerticalClamp 안으로 끌어 당김.
			const float ZOffset = MyLoc.Z - SpawnZ;
			if (FMath::Abs(ZOffset) > GhostVerticalClamp)
			{
				Dir.Z = ZOffset > 0.0f ? -0.5f : 0.5f;
				Dir = Dir.GetSafeNormal();
			}
			AddMovementInput(Dir, 0.6f);
			return;
		}
	}

	// 2) 평소엔 스폰 근처로 랜덤 부유 배회.
	const FVector MyLoc = GetActorLocation();
	const float ReachThresholdSq = FMath::Square(140.0f);
	if (!bHasGhostWanderGoal ||
		GhostWanderRetargetCooldown <= 0.0f ||
		FVector::DistSquared(MyLoc, CurrentGhostWanderGoal) <= ReachThresholdSq)
	{
		if (PickGhostWanderTarget(CurrentGhostWanderGoal))
		{
			bHasGhostWanderGoal = true;
			GhostWanderRetargetCooldown = GhostWanderRetargetInterval;
		}
	}

	if (bHasGhostWanderGoal)
	{
		const FVector Dir = (CurrentGhostWanderGoal - MyLoc).GetSafeNormal();
		AddMovementInput(Dir, 0.4f);
	}
}

bool AScreamEnemy::PickGhostWanderTarget(FVector& OutTarget) const
{
	const FVector Origin = GetActorLocation();
	const float R = FMath::Max(100.0f, GhostWanderRadius);
	OutTarget = Origin + FVector(
		FMath::FRandRange(-R, R),
		FMath::FRandRange(-R, R),
		FMath::FRandRange(-GhostVerticalClamp, GhostVerticalClamp));
	OutTarget.Z = FMath::Clamp(OutTarget.Z, SpawnZ - GhostVerticalClamp, SpawnZ + GhostVerticalClamp);
	return true;
}

void AScreamEnemy::FaceTarget(float DeltaSeconds, float RotateRateDegPerSec)
{
	if (!IsValid(TargetActor))
	{
		return;
	}

	const FVector ToT = TargetActor->GetActorLocation() - GetActorLocation();
	if (ToT.IsNearlyZero())
	{
		return;
	}

	const FRotator Goal(0.0f, FMath::RadiansToDegrees(FMath::Atan2(ToT.Y, ToT.X)), 0.0f);
	if (RotateRateDegPerSec <= KINDA_SMALL_NUMBER)
	{
		SetActorRotation(Goal);
		return;
	}

	const FRotator Cur = GetActorRotation();
	const FRotator New = FMath::RInterpConstantTo(Cur, Goal, DeltaSeconds, RotateRateDegPerSec);
	SetActorRotation(New);
}

void AScreamEnemy::EnableGhostMovement(bool bEnable)
{
	if (bEnable == bGhostActive)
	{
		return;
	}

	UCapsuleComponent* Cap = GetCapsuleComponent();
	USkeletalMeshComponent* MeshComp = GetMesh();
	UCharacterMovementComponent* Move = GetCharacterMovement();

	if (bEnable)
	{
		if (Cap)
		{
			CachedWorldStaticResponse = Cap->GetCollisionResponseToChannel(ECC_WorldStatic);
			Cap->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Ignore);
		}
		if (MeshComp)
		{
			CachedMeshWorldStaticResponse = MeshComp->GetCollisionResponseToChannel(ECC_WorldStatic);
			MeshComp->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Ignore);
		}
		if (Move)
		{
			CachedMovementMode = static_cast<uint8>(Move->MovementMode);
			CachedMaxFlySpeed = Move->MaxFlySpeed;
			Move->SetMovementMode(MOVE_Flying);
			Move->MaxFlySpeed = FMath::Max(GhostFlySpeed, 1.0f);
			Move->BrakingDecelerationFlying = 1024.0f;
		}
		bGhostActive = true;
	}
	else
	{
		if (Cap)
		{
			Cap->SetCollisionResponseToChannel(ECC_WorldStatic, CachedWorldStaticResponse);
		}
		if (MeshComp)
		{
			MeshComp->SetCollisionResponseToChannel(ECC_WorldStatic, CachedMeshWorldStaticResponse);
		}
		if (Move)
		{
			Move->SetMovementMode(static_cast<EMovementMode>(CachedMovementMode));
			Move->MaxFlySpeed = CachedMaxFlySpeed;
		}
		bGhostActive = false;
	}
}

void AScreamEnemy::TransitionToPhase(EScreamAbilityPhase NewPhase)
{
	if (NewPhase == AbilityPhase)
	{
		return;
	}
	AbilityPhase = NewPhase;
	PhaseElapsed = 0.0f;
	OnAbilityPhaseChanged.Broadcast(this, NewPhase);
}

// =============================================================================
// 이동 오버라이드 — 능력 활성 페이즈 / Ghost 쿨다운 동안 베이스의 이동을 봉인.
// =============================================================================

void AScreamEnemy::UpdateChase()
{
	if (IsAbilityActive())
	{
		StopEnemyMovement();
		return;
	}
	if (AbilityPhase == EScreamAbilityPhase::Cooldown && bGhostActive)
	{
		// Ghost 비행 배회는 TickCooldown 이 직접 운영한다. 베이스 MoveTo 는 NavMesh 와 충돌.
		return;
	}
	Super::UpdateChase();
}

void AScreamEnemy::UpdateAttack()
{
	if (IsAbilityActive())
	{
		StopEnemyMovement();
		return;
	}
	if (AbilityPhase == EScreamAbilityPhase::Cooldown && bGhostActive)
	{
		return;
	}
	Super::UpdateAttack();
}

void AScreamEnemy::UpdateIdle(float DeltaSeconds)
{
	if (IsAbilityActive())
	{
		StopEnemyMovement();
		return;
	}
	if (AbilityPhase == EScreamAbilityPhase::Cooldown && bGhostActive)
	{
		return;
	}
	Super::UpdateIdle(DeltaSeconds);
}

// =============================================================================
// CC 처리 — 일반 전투 경직은 무시하고, 광원 누적 경직만 인정한다.
//   · ApplyCCStun(전투계 호출): 빈 함수.
//   · OnLightHit: 누적값 ↑, 임계 도달 시 TriggerLightStun.
//   · TriggerLightStun: 베이스의 ApplyCCStun(LightStunDuration) 을 직접 호출해 인프라 재사용.
// =============================================================================

void AScreamEnemy::ApplyCCStun(float /*Duration*/)
{
	// 전투에서 보내는 일반 경직은 무시. 본 적은 광원 누적 경직만 받는다.
	// 내부 라이트 스턴은 AEnemyBase::ApplyCCStun(...) 을 명시적으로 호출.
}

void AScreamEnemy::OnLightHit(float Intensity, float Duration)
{
	// 베이스에 위임 — Whisper 의 슬로우 처리, 이벤트 브로드캐스트 등은 그대로 살린다.
	Super::OnLightHit(Intensity, Duration);

	if (!IsAlive())
	{
		return;
	}

	// Scream은 "빛에 맞은 시간"이 핵심이므로 intensity 가 아주 낮아도 호출이 들어왔으면 누적한다.
	// 거리 감쇠가 0에 가까운 끝자락에서도 판정이 닿았다면 최소 노출량으로 본다.
	float ClampedDuration = FMath::Max(0.0f, Duration);
	if (ClampedDuration <= 0.0f)
	{
		ClampedDuration = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;
	}
	ClampedDuration = FMath::Max(ClampedDuration, 0.05f);
	if (ClampedDuration <= 0.0f)
	{
		return;
	}

	// 면역 윈도(스턴 지속 포함) 동안에는 누적값을 올리지 않는다.
	if (IsLightImmune())
	{
		return;
	}

	if (UWorld* W = GetWorld())
	{
		LastLightHitWorldTime = W->GetTimeSeconds();
	}

	// 누적은 단순 시간 합산. Intensity 는 이벤트/디버그용으로만 의미를 둔다.
	LightExposureAccum += ClampedDuration;

	if (LightExposureAccum >= LightStunBuildupSeconds)
	{
		TriggerLightStun();
	}
}

void AScreamEnemy::UpdateLightStunState(float DeltaSeconds)
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}

	const double Now = W->GetTimeSeconds();

	// Grace 만료 시 누적값 빠르게 감소(빛이 비추지 않으면 곧바로 0 으로).
	if (Now - LastLightHitWorldTime > LightExposureGraceSeconds && LightExposureAccum > 0.0f)
	{
		const float DecayPerSec = LightStunBuildupSeconds * FMath::Max(0.0f, LightExposureDecayRate);
		LightExposureAccum = FMath::Max(0.0f, LightExposureAccum - DecayPerSec * DeltaSeconds);
	}
}

void AScreamEnemy::TriggerLightStun()
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}

	const double Now = W->GetTimeSeconds();
	LightStunUntilSec = Now + LightStunDuration;
	// 면역 윈도는 스턴 종료 직후부터 LightStunImmunitySeconds.
	LightImmuneUntilSec = LightStunUntilSec + LightStunImmunitySeconds;
	LightExposureAccum = 0.0f;

	// 차지/체인포즈 중이라면 능력을 즉시 캔슬해 Cooldown 으로 → 플레이어 명확한 보상.
	// Transit/Root 중에는 능력 진행만 잠시 멈춘다(추가 캔슬 없음).
	if (AbilityPhase == EScreamAbilityPhase::Charging ||
		AbilityPhase == EScreamAbilityPhase::ChainPause)
	{
		StartCooldown();
	}

	// 베이스의 CC 인프라 재사용(애니/이동 정지 + 타이머 + bCCStunned).
	AEnemyBase::ApplyCCStun(LightStunDuration);

#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Warning, TEXT("Scream light stun triggered: %s duration=%.2f immunity=%.2f"),
		*GetNameSafe(this),
		LightStunDuration,
		LightStunImmunitySeconds);
#endif
}

bool AScreamEnemy::IsLightStunActive() const
{
	const UWorld* W = GetWorld();
	if (!W)
	{
		return false;
	}
	return W->GetTimeSeconds() < LightStunUntilSec;
}

bool AScreamEnemy::IsLightImmune() const
{
	const UWorld* W = GetWorld();
	if (!W)
	{
		return false;
	}
	// 스턴 진행 중에도 면역 윈도가 활성으로 잡히도록 LightImmuneUntilSec >= LightStunUntilSec.
	return W->GetTimeSeconds() < LightImmuneUntilSec;
}

// =============================================================================
// 페이즈 2(체력 50% 이하) — 더블 블링크 활성화.
// 한 번 페이즈 2 가 되면 회복돼도 유지(스파이크 방지).
// =============================================================================

void AScreamEnemy::NotifyEnemyDamageApplied(float AppliedDamage)
{
	Super::NotifyEnemyDamageApplied(AppliedDamage);
	UpdateHealthPhase();
}

void AScreamEnemy::UpdateHealthPhase()
{
	if (bIsPhaseTwo)
	{
		return; // sticky
	}

	const float Pct = MaxHealth > 0.0f ? CurrentHealth / MaxHealth : 0.0f;
	if (Pct <= PhaseTwoHealthPercent)
	{
		bIsPhaseTwo = true;

		// 이미 차지 중이면 즉시 더블 블링크 사이클로 보정.
		if (AbilityPhase == EScreamAbilityPhase::Charging && BlinksRemainingInCycle == 1)
		{
			BlinksRemainingInCycle = FMath::Max(1, PhaseTwoBlinksPerCycle);
		}
	}
}
