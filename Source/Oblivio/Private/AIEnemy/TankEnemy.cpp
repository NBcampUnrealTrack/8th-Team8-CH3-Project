#include "AIEnemy/TankEnemy.h"

#include "Combat/TankHeartbeatDamageType.h"
#include "Combat/TankJumpAttackDamageType.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Engine/World.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ATankEnemy::ATankEnemy()
{
	// Basic 대비 대략 3배 체력, ~63% 이속, 데미지+5, 약간 느린 공격
	MaxHealth = 300.0f;
	CurrentHealth = MaxHealth;
	MoveSpeed = 220.0f;
	ChaseMoveSpeed = 280.0f;
	AttackDamage = 15.0f;
	AttackRange = 200.0f;
	AttackCooldown = 1.25f;
	ChaseAcceptanceRadius = 55.0f;
	ChaseProximityBuffer = 48.0f;
	AggroRadius = 1000.0f;
	CorpseLifeSpan = 3.0f;

	TankHeartMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TankHeartMesh"));
	TankHeartMeshComponent->SetupAttachment(GetMesh());
	TankHeartMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TankHeartMeshComponent->SetCastShadow(true);
	TankHeartMeshComponent->SetVisibility(false);

	TankHeartPulseLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("TankHeartPulseLight"));
	TankHeartPulseLight->SetupAttachment(TankHeartMeshComponent);
	TankHeartPulseLight->SetMobility(EComponentMobility::Movable);
	TankHeartPulseLight->CastShadows = false;
	TankHeartPulseLight->bAffectsWorld = true;
	TankHeartPulseLight->SetIntensity(0.f);
	TankHeartPulseLight->SetLightColor(FLinearColor(1.f, 0.12f, 0.08f));
	TankHeartPulseLight->SetAttenuationRadius(HeartPulseLightAttenuationRadius);

	HeartbeatAoERangeIndicatorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeartbeatAoERangeIndicator"));
	if (HeartbeatAoERangeIndicatorMesh)
	{
		HeartbeatAoERangeIndicatorMesh->SetupAttachment(RootComponent);
		HeartbeatAoERangeIndicatorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		HeartbeatAoERangeIndicatorMesh->SetGenerateOverlapEvents(false);
		HeartbeatAoERangeIndicatorMesh->SetCastShadow(false);
		HeartbeatAoERangeIndicatorMesh->SetHiddenInGame(true);

		static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderAsset(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		if (CylinderAsset.Succeeded())
		{
			HeartbeatAoERangeIndicatorMesh->SetStaticMesh(CylinderAsset.Object);
		}
	}

	JumpLandingAoERangeIndicatorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("JumpLandingAoERangeIndicator"));
	if (JumpLandingAoERangeIndicatorMesh)
	{
		JumpLandingAoERangeIndicatorMesh->SetupAttachment(RootComponent);
		JumpLandingAoERangeIndicatorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		JumpLandingAoERangeIndicatorMesh->SetGenerateOverlapEvents(false);
		JumpLandingAoERangeIndicatorMesh->SetCastShadow(false);
		JumpLandingAoERangeIndicatorMesh->SetHiddenInGame(true);

		static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderJump(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		if (CylinderJump.Succeeded())
		{
			JumpLandingAoERangeIndicatorMesh->SetStaticMesh(CylinderJump.Object);
		}
	}
}

void ATankEnemy::SetTankHeartMeshVisible(const bool bVisible)
{
	if (!TankHeartMeshComponent)
	{
		return;
	}
	// BP/초기화 순서로 부착이 비었거나 스켈 로드 전에 BeginPlay 가 지난 경우 — 노티 시점에 다시 손 소켓에 스냅
	if (bVisible)
	{
		if (!TankHeartMeshComponent->GetStaticMesh())
		{
			if (!bLoggedTankHeartStaticMeshMissing)
			{
				bLoggedTankHeartStaticMeshMissing = true;
				UE_LOG(LogTemp, Warning,
					TEXT("%s: TankHeartMesh has no Static Mesh set on the Blueprint component — heart will not render. "
						 "Open BP_TankEnemy → Components → TankHeartMesh → Static Mesh."),
					*GetNameSafe(this));
			}
		}
		AttachTankHeartMeshToLeftHand();
	}
	TankHeartMeshComponent->SetVisibility(bVisible, true);
}

void ATankEnemy::AttachTankHeartMeshToLeftHand()
{
	if (!TankHeartMeshComponent || !GetMesh())
	{
		return;
	}

	USkeletalMeshComponent* const Skel = GetMesh();
	TArray<FName> TryOrder;
	if (!TankHeartAttachSocketName.IsNone())
	{
		TryOrder.Add(TankHeartAttachSocketName);
	}

	static const FName LeftHandCandidates[] = {
		FName(TEXT("hand_l")),
		FName(TEXT("Hand_L")),
		FName(TEXT("hand_lSocket")),
		FName(TEXT("lowerarm_l")),
		FName(TEXT("LeftHand")),
		FName(TEXT("LeftHandSocket")),
		FName(TEXT("weapon_l")),
		FName(TEXT("Weapon_L")),
		FName(TEXT("hand_l_socket")),
		FName(TEXT("Socket_Hand_L")),
	};
	for (const FName& C : LeftHandCandidates)
	{
		TryOrder.AddUnique(C);
	}

	for (const FName& SocketName : TryOrder)
	{
		if (Skel->DoesSocketExist(SocketName))
		{
			TankHeartMeshComponent->AttachToComponent(
				Skel,
				FAttachmentTransformRules::SnapToTargetNotIncludingScale,
				SocketName);
			return;
		}
	}

	UE_LOG(LogTemp, Warning,
		TEXT("%s: Tank heart — no matching socket on left hand. Set TankHeartAttachSocketName or add socket on skeleton."),
		*GetNameSafe(this));
}

void ATankEnemy::PulseTankHeartDamageFlash()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (!TankHeartPulseLight || !TankHeartMeshComponent || !TankHeartMeshComponent->IsVisible())
	{
		return;
	}

	TankHeartPulseLight->SetAttenuationRadius(HeartPulseLightAttenuationRadius);
	TankHeartPulseLight->SetLightColor(FLinearColor(1.f, 0.12f, 0.08f));
	HeartPulseLightFadeCurrent = FMath::Max(0.f, HeartPulseLightPeakCandela);
	TankHeartPulseLight->SetIntensity(HeartPulseLightFadeCurrent);
	TankHeartPulseLight->SetVisibility(true);

	if (UWorld* const W = GetWorld())
	{
		W->GetTimerManager().ClearTimer(HeartPulseFadeTimerHandle);
		W->GetTimerManager().SetTimer(
			HeartPulseFadeTimerHandle,
			this,
			&ATankEnemy::TickHeartPulseLightFade,
			FMath::Max(0.01f, HeartPulseFadeTickSeconds),
			true);
	}
}

void ATankEnemy::TickHeartPulseLightFade()
{
	if (!TankHeartPulseLight)
	{
		ClearHeartPulseFlashTimer();
		return;
	}
	const float I = HeartPulseLightFadeCurrent;
	const float NewI = I * HeartPulseFadeMultiplierPerTick;
	HeartPulseLightFadeCurrent = NewI;
	if (NewI <= HeartPulseFadeStopBelowCandela)
	{
		HeartPulseLightFadeCurrent = 0.f;
		TankHeartPulseLight->SetIntensity(0.f);
		ClearHeartPulseFlashTimer();
		return;
	}
	TankHeartPulseLight->SetIntensity(NewI);
}

void ATankEnemy::ClearHeartPulseFlashTimer()
{
	if (UWorld* const W = GetWorld())
	{
		W->GetTimerManager().ClearTimer(HeartPulseFadeTimerHandle);
	}
	if (TankHeartPulseLight)
	{
		TankHeartPulseLight->SetIntensity(0.f);
	}
	HeartPulseLightFadeCurrent = 0.f;
}

void ATankEnemy::BeginPlay()
{
	Super::BeginPlay();
	AttachTankHeartMeshToLeftHand();
	// BP Class Defaults에 옛날에 false가 저장돼 있으면 C++ 기본 true를 덮어써 심작이 영구히 꺼진 것처럼 보임 — 런타임에는 탱커 기본 동작으로 다시 켬.
	bUseHeartbeatAoEAttack = true;
	bUseTankJumpAttackAfterHeartbeat = true;
	// 심장 표시 시점은 TankHeartShow 애님 노티 전용(채널링 시작과 분리)
	SetTankHeartMeshVisible(false);

	if (HeartbeatAoERangeIndicatorMesh && HeartbeatAoERangeIndicatorMaterial)
	{
		HeartbeatAoERangeIndicatorMesh->SetMaterial(0, HeartbeatAoERangeIndicatorMaterial);
	}
	if (JumpLandingAoERangeIndicatorMesh && JumpLandingAoERangeIndicatorMaterial)
	{
		JumpLandingAoERangeIndicatorMesh->SetMaterial(0, JumpLandingAoERangeIndicatorMaterial);
	}
}

void ATankEnemy::OnRep_HeartbeatChanneling()
{
	// 표시는 애님 TankHeartShow 노티 — 여기서는 채널링 종료 시에만 숨김(히든 노티 누락 보완)
	if (!bHeartbeatChanneling)
	{
		SetTankHeartMeshVisible(false);
	}
}

void ATankEnemy::OnRep_TankJumpAttackActive()
{
	RefreshIdleChaseLocomotionAmbientFromCurrentDisplayState();
}

void ATankEnemy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATankEnemy, bHeartbeatChanneling);
	DOREPLIFETIME(ATankEnemy, bTankStickyAggroUntilDeath);
	DOREPLIFETIME(ATankEnemy, bTankJumpAttackActive);
	DOREPLIFETIME(ATankEnemy, bTankJumpShowLandingTelegraph);
	DOREPLIFETIME(ATankEnemy, TankJumpLandingFloorWorld);
}

bool ATankEnemy::HasValidAggroTarget() const
{
	if (!IsValid(TargetActor))
	{
		return false;
	}

	if (bTankStickyAggroUntilDeath)
	{
		return true;
	}

	if (Super::HasValidAggroTarget())
	{
		return true;
	}

	if (!bUseHeartbeatAoEAttack)
	{
		return false;
	}

	return IsTargetInHeartbeatAoERange();
}

EEnemyAIState ATankEnemy::SelectStateWhileAggroed() const
{
	if (bTankJumpAttackActive && IsAlive())
	{
		return EEnemyAIState::JumpAttack;
	}
	if (bUseHeartbeatAoEAttack && bHeartbeatChanneling && IsAlive())
	{
		return EEnemyAIState::Heartbeat;
	}
	return Super::SelectStateWhileAggroed();
}

bool ATankEnemy::TryConsumeSpecialFSMUpdate()
{
	if (!IsAlive())
	{
		return false;
	}
	if (bTankJumpAttackActive)
	{
		ApplyAggroCombatTransientCleanup();
		SetEnemyState(EEnemyAIState::JumpAttack);
		return true;
	}
	if (!bUseHeartbeatAoEAttack || !bHeartbeatChanneling)
	{
		return false;
	}
	ApplyAggroCombatTransientCleanup();
	SetEnemyState(EEnemyAIState::Heartbeat);
	return true;
}

bool ATankEnemy::UsesHeartbeatAoEAttack() const
{
	return bUseHeartbeatAoEAttack;
}

EEnemyAIState ATankEnemy::GetEnemyState() const
{
	if (IsCCStunned())
	{
		return EEnemyAIState::Stunned;
	}
	if (bTankJumpAttackActive)
	{
		return EEnemyAIState::JumpAttack;
	}
	if (bUseHeartbeatAoEAttack && bHeartbeatChanneling)
	{
		return EEnemyAIState::Heartbeat;
	}
	return EnemyState;
}

void ATankEnemy::ApplyHeartbeatDamageFromAnimNotify()
{
	if (!bUseHeartbeatAoEAttack)
	{
		return;
	}
	if (!HasAuthority() || !IsAlive())
	{
		return;
	}
	if (!bHeartbeatChanneling)
	{
		return;
	}
	/** 점프 패턴 재생 중(몽타주 중첩·노티 순서 꼬임)일 때 심작 박동 피해 무시 */
	if (bTankJumpAttackActive)
	{
		return;
	}
	if (HeartbeatPulseDamage <= KINDA_SMALL_NUMBER || !IsValid(TargetActor))
	{
		return;
	}
	if (!IsTargetInHeartbeatDamageRange())
	{
		return;
	}
	DispatchEnemyAttackCommitted(TargetActor, HeartbeatPulseDamage, UTankHeartbeatDamageType::StaticClass());
}

void ATankEnemy::FinishHeartbeatAttackFromAnimNotify()
{
	if (!HasAuthority())
	{
		return;
	}
	FinishHeartbeatSequence();
}

bool ATankEnemy::IsTankHeartbeatChannelingForAnim() const
{
	return bUseHeartbeatAoEAttack && bHeartbeatChanneling && IsAlive();
}

void ATankEnemy::Die()
{
	bTankStickyAggroUntilDeath = false;

	ClearTankJumpTimers();
	bTankJumpAttackActive = false;
	bTankJumpShowLandingTelegraph = false;
	bTankJumpKinematicAscent_Server = false;
	bTankJumpLandingDamageCommitted_Server = false;

	if (HeartbeatAoERangeIndicatorMesh)
	{
		HeartbeatAoERangeIndicatorMesh->SetVisibility(false);
		HeartbeatAoERangeIndicatorMesh->SetHiddenInGame(true);
	}
	if (JumpLandingAoERangeIndicatorMesh)
	{
		JumpLandingAoERangeIndicatorMesh->SetVisibility(false);
		JumpLandingAoERangeIndicatorMesh->SetHiddenInGame(true);
	}

	SetTankHeartMeshVisible(false);
	ClearHeartPulseFlashTimer();
	ClearHeartbeatTimers();
	bHeartbeatChanneling = false;
	Super::Die();
}

void ATankEnemy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearHeartPulseFlashTimer();
	ClearHeartbeatTimers();
	ClearTankJumpTimers();
	bHeartbeatChanneling = false;
	bTankStickyAggroUntilDeath = false;
	bTankJumpAttackActive = false;
	bTankJumpShowLandingTelegraph = false;

	if (HeartbeatAoERangeIndicatorMesh)
	{
		HeartbeatAoERangeIndicatorMesh->SetVisibility(false);
		HeartbeatAoERangeIndicatorMesh->SetHiddenInGame(true);
	}
	if (JumpLandingAoERangeIndicatorMesh)
	{
		JumpLandingAoERangeIndicatorMesh->SetVisibility(false);
		JumpLandingAoERangeIndicatorMesh->SetHiddenInGame(true);
	}

	Super::EndPlay(EndPlayReason);
}

void ATankEnemy::Tick(float DeltaSeconds)
{
	if (HasAuthority())
	{
		if (!IsValid(TargetActor))
		{
			FindDefaultTarget();
		}
		if (!IsValid(TargetActor))
		{
			bTankStickyAggroUntilDeath = false;
		}
		else if (IsAlive() && !bTankStickyAggroUntilDeath && IsAggroDistanceSatisfiedForTarget())
		{
			bTankStickyAggroUntilDeath = true;
		}
		if (HasAuthority())
		{
			TickTankJumpArc_Server();
		}
	}

	// ALuxeaterEnemy: TickBossAbilities 가 Super::Tick 보다 먼저 — 심작은 FSM/이동보다 앞에서도 한 번 시도
	if (bUseHeartbeatAoEAttack)
	{
		if (!IsValid(TargetActor))
		{
			FindDefaultTarget();
		}
		MaybeTryTankHeartbeatAoE();
	}

	Super::Tick(DeltaSeconds);

	UpdateHeartbeatAoERangeIndicatorVisual();
	UpdateJumpLandingAoEIndicatorVisual();

	if (bUseHeartbeatAoEAttack)
	{
		// Chase 중 장애물로 UpdateChase 가 생략될 때 등, 같은 틱 후반 보강
		MaybeTryTankHeartbeatAoE();
	}
}

void ATankEnemy::UpdateChase()
{
	Super::UpdateChase();
}

void ATankEnemy::MaybeTryTankHeartbeatAoE()
{
	if (!bUseHeartbeatAoEAttack)
	{
		return;
	}

	if (bTankJumpAttackActive)
	{
		return;
	}

	if (!IsAlive() || IsCCStunned())
	{
		return;
	}

	if (bHeartbeatChanneling)
	{
		return;
	}

	if (ShouldSuppressAILocomotion())
	{
		return;
	}

	if (!GetWorld() || !HasAuthority())
	{
		return;
	}

	if (!HasValidAggroTarget() || !IsValid(TargetActor))
	{
		return;
	}

	if (IsTargetInAttackRange())
	{
		return;
	}

	TryStartHeartbeatWhenReady();
}

bool ATankEnemy::TryStartHeartbeatWhenReady()
{
	if (!bUseHeartbeatAoEAttack || bHeartbeatChanneling)
	{
		return false;
	}
	if (bTankJumpAttackActive)
	{
		return false;
	}

	UWorld* const World = GetWorld();
	if (!World || !HasAuthority() || !IsAlive())
	{
		return false;
	}

	if (!IsValid(TargetActor))
	{
		return false;
	}

	// 근접 반경(AttackRange) 안에서는 베이스 근공만 — 심작은 그 밖에서만
	if (IsTargetInAttackRange())
	{
		return false;
	}

	const float Now = World->GetTimeSeconds();
	if (!IsHeartbeatCooldownReady(Now) || !IsTargetInHeartbeatAoERange())
	{
		return false;
	}

	TryStartHeartbeatSequence();
	return true;
}

void ATankEnemy::DrawDebugCombatExtras()
{
#if UE_BUILD_SHIPPING
	return;
#else
	if (!bDebugDrawCombatRanges || !GetWorld())
	{
		return;
	}

	const FVector Loc = GetActorLocation();
	UWorld* const W = GetWorld();

	if (AttackRange > KINDA_SMALL_NUMBER)
	{
		const FColor C = FColor::Cyan;
		DrawDebugSphere(W, Loc, AttackRange, 24, C, false, 0.f, 0, 1.5f);
		const FVector LabelPos = Loc + FVector(120.f, 0.f, FMath::Max(80.f, AttackRange * 0.2f));
		DrawDebugString(W, LabelPos, FString::Printf(TEXT("Melee %.0f cm"), AttackRange), nullptr, C, 0.f, true, 0.95f);
	}

	if (bUseHeartbeatAoEAttack && HeartbeatAoERadius > KINDA_SMALL_NUMBER)
	{
		const FColor H = FColor::Magenta;
		DrawDebugSphere(W, Loc, HeartbeatAoERadius, 36, H, false, 0.f, 0, 1.2f);
		const FVector HLabel = Loc + FVector(-120.f, 0.f, FMath::Max(100.f, HeartbeatAoERadius * 0.2f));
		DrawDebugString(W, HLabel,
			FString::Printf(TEXT("Heartbeat %.0f cm %s"), HeartbeatAoERadius,
				bHeartbeatUseHorizontalDistance ? TEXT("(XY)") : TEXT("(3D)")),
			nullptr, H, 0.f, true, 0.95f);
	}
#endif
}

bool ATankEnemy::ShouldSuppressAILocomotion() const
{
	if (bTankJumpAttackActive)
	{
		return true;
	}
	if (bUseHeartbeatAoEAttack && bHeartbeatChanneling)
	{
		return true;
	}
	return Super::ShouldSuppressAILocomotion();
}

bool ATankEnemy::IsHeartbeatCooldownReady(const float NowWorldSeconds) const
{
	if (HeartbeatCooldownSeconds <= KINDA_SMALL_NUMBER)
	{
		return true;
	}
	return (NowWorldSeconds - LastHeartbeatSequenceEndWorldTime) >= HeartbeatCooldownSeconds;
}

bool ATankEnemy::IsTargetInHeartbeatAoERange() const
{
	if (!IsValid(TargetActor))
	{
		return false;
	}

	const FVector SelfLoc = GetActorLocation();
	const FVector TargetLoc = TargetActor->GetActorLocation();
	const float R = HeartbeatAoERadius;
	const float SqR = R * R;

	if (bHeartbeatUseHorizontalDistance)
	{
		const float Dx = TargetLoc.X - SelfLoc.X;
		const float Dy = TargetLoc.Y - SelfLoc.Y;
		return (Dx * Dx + Dy * Dy) <= SqR;
	}

	return FVector::DistSquared(SelfLoc, TargetLoc) <= SqR;
}

bool ATankEnemy::IsTargetInHeartbeatDamageRange() const
{
	return IsTargetInHeartbeatAoERange();
}

void ATankEnemy::TryStartHeartbeatSequence()
{
	UWorld* const World = GetWorld();
	if (!World || !HasAuthority())
	{
		return;
	}
	if (bTankJumpAttackActive)
	{
		return;
	}
	if (HeartbeatPulseDamage <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	ClearHeartbeatTimers();
	bHeartbeatChanneling = true;
	// 새 심작 구간 시작 시 심장은 숨긴 채로 — TankHeartShow 노티 시점에만 표시
	SetTankHeartMeshVisible(false);
	// UpdateState 는 이미 틱 초반에 지나감 — 채널링 직후 Chase→Heartbeat FSM 즉시 반영
	SetEnemyState(SelectStateWhileAggroed());

	// 피해는 애님 노티만(Tank Heartbeat Damage). 종료는 끝 노티 또는 Fail-Safe.
	if (HeartbeatChannelFailSafeSeconds > KINDA_SMALL_NUMBER)
	{
		World->GetTimerManager().SetTimer(
			HeartbeatFailSafeTimerHandle,
			this,
			&ATankEnemy::FinishHeartbeatSequence,
			HeartbeatChannelFailSafeSeconds,
			false);
	}
}

void ATankEnemy::FinishHeartbeatSequence()
{
	ClearHeartbeatTimers();

	if (HasAuthority())
	{
		if (USkeletalMeshComponent* const Sk = GetMesh())
		{
			if (UAnimInstance* const AI = Sk->GetAnimInstance())
			{
				AI->StopAllMontages(0.08f);
			}
		}
	}

	bHeartbeatChanneling = false;
	// 서버 전용 종료 경로이므로 권한 없는 머신은 OnRep 로 끔
	SetTankHeartMeshVisible(false);
	if (UWorld* const W = GetWorld())
	{
		LastHeartbeatSequenceEndWorldTime = W->GetTimeSeconds();
	}

	if (IsAlive())
	{
		if (!TryBeginTankJumpAttackAfterHeartbeat())
		{
			SetEnemyState(SelectStateWhileAggroed(), true);
		}
	}
}

void ATankEnemy::ClearHeartbeatTimers()
{
	if (UWorld* const W = GetWorld())
	{
		W->GetTimerManager().ClearTimer(HeartbeatFailSafeTimerHandle);
	}
}

void ATankEnemy::UpdateHeartbeatAoERangeIndicatorVisual()
{
	if (!HeartbeatAoERangeIndicatorMesh)
	{
		return;
	}

	const bool bShouldShow = bShowHeartbeatAoERangeIndicator && bUseHeartbeatAoEAttack && bHeartbeatChanneling && IsAlive()
		&& IsValid(HeartbeatAoERangeIndicatorMaterial);

	if (!bShouldShow || HeartbeatAoERadius <= KINDA_SMALL_NUMBER
		|| HeartbeatAoERangeIndicatorBuiltInSphereRadiusUU <= KINDA_SMALL_NUMBER)
	{
		HeartbeatAoERangeIndicatorMesh->SetVisibility(false);
		HeartbeatAoERangeIndicatorMesh->SetHiddenInGame(true);
		return;
	}

	if (const UCapsuleComponent* Cap = GetCapsuleComponent())
	{
		HeartbeatAoERangeIndicatorMesh->SetRelativeLocation(FVector(0.f, 0.f, -Cap->GetScaledCapsuleHalfHeight()));
	}

	const float XYScale = HeartbeatAoERadius / HeartbeatAoERangeIndicatorBuiltInSphereRadiusUU;
	const float ZScale = FMath::Clamp(HeartbeatAoERangeIndicatorDiskThicknessScale, 0.001f, 2.f);
	HeartbeatAoERangeIndicatorMesh->SetRelativeScale3D(FVector(XYScale, XYScale, ZScale));
	HeartbeatAoERangeIndicatorMesh->SetHiddenInGame(false);
	HeartbeatAoERangeIndicatorMesh->SetVisibility(true);
}

bool ATankEnemy::IsTankJumpAttackFsmActiveForAnim() const
{
	return bTankJumpAttackActive && IsAlive();
}

bool ATankEnemy::TryBeginTankJumpAttackAfterHeartbeat()
{
	if (!HasAuthority())
	{
		return false;
	}
	if (!bUseTankJumpAttackAfterHeartbeat || !IsValid(TankJumpAttackMontage))
	{
		return false;
	}
	if (!IsAlive() || IsCCStunned())
	{
		return false;
	}
	// 심작 AoE 거리 무관: 종료 후에는 TargetActor 만 있으면 점프(HasValidAggroTarget 는 AoE 포함)
	if (!IsValid(TargetActor))
	{
		return false;
	}

	return StartTankJumpAttackSequence_Server();
}

bool ATankEnemy::StartTankJumpAttackSequence_Server()
{
	UWorld* const World = GetWorld();
	if (!World || !HasAuthority() || !IsAlive())
	{
		return false;
	}
	if (bTankJumpAttackActive || bHeartbeatChanneling)
	{
		return false;
	}
	if (!IsValid(TankJumpAttackMontage))
	{
		return false;
	}
	if (!IsValid(TargetActor))
	{
		return false;
	}

	ClearTankJumpTimers();

	bTankJumpAttackActive = true;
	bTankJumpKinematicAscent_Server = false;
	bTankJumpLandingDamageCommitted_Server = false;
	TankJumpLiftOffStampServerSecs = -1.e20;
	TankJumpArcBeginWorld = GetActorLocation();

	// 심박 직후·몽타주 선딜 동안 플레이 근처(캡슐 겹침 회피) 바닥으로 착지점 고정 + 텔레그래프
	const FVector ProbeLoc = ComputeJumpLandingFeetProbeWorld_Server();
	SnapshotJumpLandingFloorFromActor_Server(ProbeLoc, TargetActor.Get());
	bTankJumpShowLandingTelegraph = true;

	SetEnemyState(EEnemyAIState::JumpAttack, true);
	Multicast_PlayTankJumpMontage();

	if (TankJumpFailsafeSeconds > KINDA_SMALL_NUMBER)
	{
		World->GetTimerManager().SetTimer(
			TankJumpFailsafeTimerHandle,
			this,
			&ATankEnemy::OnTankJumpFailsafe_Server,
			TankJumpFailsafeSeconds,
			false);
	}

	if (TankJumpForcedLiftOffAfterMontageStartsSeconds > KINDA_SMALL_NUMBER)
	{
		World->GetTimerManager().SetTimer(
			TankJumpLiftOffFailsafeTimerHandle,
			this,
			&ATankEnemy::TankJumpLiftOffFailsafe_Server,
			TankJumpForcedLiftOffAfterMontageStartsSeconds,
			false);
	}

	if (TankJumpForcedLandingAfterMontageStartsSeconds > KINDA_SMALL_NUMBER)
	{
		World->GetTimerManager().SetTimer(
			TankJumpLandingFailsafeTimerHandle,
			this,
			&ATankEnemy::TankJumpLandingFailsafe_Server,
			TankJumpForcedLandingAfterMontageStartsSeconds,
			false);
	}

	return true;
}

void ATankEnemy::ClearTankJumpTimers()
{
	if (UWorld* const W = GetWorld())
	{
		W->GetTimerManager().ClearTimer(TankJumpFailsafeTimerHandle);
		W->GetTimerManager().ClearTimer(TankJumpLiftOffFailsafeTimerHandle);
		W->GetTimerManager().ClearTimer(TankJumpLandingFailsafeTimerHandle);
	}
}

void ATankEnemy::OnTankJumpFailsafe_Server()
{
	if (!HasAuthority() || !bTankJumpAttackActive)
	{
		return;
	}
	if (!bTankJumpLandingDamageCommitted_Server)
	{
		if (!bTankJumpKinematicAscent_Server)
		{
			LiftOffJumpAttack_Server_Impl();
		}
		ApplyJumpSlamAndRing_Server(FVector(TankJumpLandingFloorWorld));
	}
	CompleteTankJumpAttackSequence_Server();
}

void ATankEnemy::TankJumpLiftOffFailsafe_Server()
{
	if (!HasAuthority() || !bTankJumpAttackActive)
	{
		return;
	}
	if (bTankJumpKinematicAscent_Server)
	{
		return;
	}
	LiftOffJumpAttack_Server_Impl();
}

void ATankEnemy::TankJumpLandingFailsafe_Server()
{
	if (!HasAuthority() || !bTankJumpAttackActive)
	{
		return;
	}
	if (bTankJumpLandingDamageCommitted_Server)
	{
		return;
	}
	if (!bTankJumpKinematicAscent_Server)
	{
		LiftOffJumpAttack_Server_Impl();
	}
	ApplyJumpSlamAndRing_Server(FVector(TankJumpLandingFloorWorld));
}

void ATankEnemy::CompleteTankJumpAttackSequence_Server()
{
	if (!HasAuthority())
	{
		return;
	}
	ClearTankJumpTimers();
	bTankJumpAttackActive = false;
	bTankJumpShowLandingTelegraph = false;
	bTankJumpKinematicAscent_Server = false;
	bTankJumpLandingDamageCommitted_Server = false;

	if (JumpLandingAoERangeIndicatorMesh)
	{
		JumpLandingAoERangeIndicatorMesh->SetVisibility(false);
		JumpLandingAoERangeIndicatorMesh->SetHiddenInGame(true);
	}

	if (UCharacterMovementComponent* const Move = GetCharacterMovement())
	{
		if (Move->IsFlying())
		{
			Move->SetMovementMode(MOVE_Walking);
		}
	}

	if (IsAlive())
	{
		SetEnemyState(SelectStateWhileAggroed(), true);
	}
}

void ATankEnemy::JumpAttack_NotifyLiftOff()
{
	if (!HasAuthority() || !bTankJumpAttackActive)
	{
		return;
	}
	LiftOffJumpAttack_Server_Impl();
}

void ATankEnemy::JumpAttack_NotifyLandingImpact()
{
	if (!HasAuthority() || !bTankJumpAttackActive)
	{
		return;
	}
	if (bHeartbeatChanneling)
	{
		return;
	}
	if (UWorld* const W = GetWorld())
	{
		W->GetTimerManager().ClearTimer(TankJumpLandingFailsafeTimerHandle);
	}
	ApplyJumpSlamAndRing_Server(FVector(TankJumpLandingFloorWorld));
}

void ATankEnemy::JumpAttack_NotifyMontageFinished()
{
	if (!HasAuthority())
	{
		return;
	}
	if (!bTankJumpAttackActive)
	{
		return;
	}
	CompleteTankJumpAttackSequence_Server();
}

void ATankEnemy::LiftOffJumpAttack_Server_Impl()
{
	if (!HasAuthority() || !bTankJumpAttackActive || bTankJumpKinematicAscent_Server)
	{
		return;
	}
	UWorld* const World = GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().ClearTimer(TankJumpLiftOffFailsafeTimerHandle);

	if (UCharacterMovementComponent* const Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->SetMovementMode(MOVE_Flying);
	}

	const FVector GroundCenter = GetActorLocation();
	const float InstantLift = FMath::Max(0.f, TankJumpLiftOffInstantZCm);
	const FVector AirStart = GroundCenter + FVector(0.f, 0.f, InstantLift);
	SetActorLocation(AirStart, false, nullptr, ETeleportType::TeleportPhysics);
	TankJumpArcBeginWorld = AirStart;
	TankJumpLiftOffStampServerSecs = World->GetTimeSeconds();

	bTankJumpKinematicAscent_Server = true;
	bTankJumpLandingDamageCommitted_Server = false;
}

FVector ATankEnemy::ComputeJumpLandingFeetProbeWorld_Server() const
{
	FVector Feet = FVector::ZeroVector;
	if (!IsValid(TargetActor))
	{
		return Feet;
	}

	Feet = TargetActor->GetActorLocation();
	if (const ACharacter* const Chr = Cast<ACharacter>(TargetActor))
	{
		if (const UCapsuleComponent* const TC = Chr->GetCapsuleComponent())
		{
			Feet.Z -= TC->GetScaledCapsuleHalfHeight();
		}
	}

	if (!bTankJumpLandingAvoidPlayerCapsuleOverlap)
	{
		return Feet;
	}

	float PlayerR = 40.f;
	if (const ACharacter* const Chr = Cast<ACharacter>(TargetActor))
	{
		if (const UCapsuleComponent* const TC = Chr->GetCapsuleComponent())
		{
			PlayerR = TC->GetScaledCapsuleRadius();
		}
	}

	const UCapsuleComponent* const TankCap = GetCapsuleComponent();
	const float TankR = TankCap ? TankCap->GetScaledCapsuleRadius() : 55.f;
	const float RequiredHorizSep = TankR + PlayerR + FMath::Max(0.f, TankJumpLandingExtraRadialGapCm);

	const FVector2D FeetXY(Feet.X, Feet.Y);
	const FVector2D TankXY(GetActorLocation().X, GetActorLocation().Y);
	FVector2D Dir = TankXY - FeetXY;
	if (Dir.SizeSquared() < 25.f)
	{
		const FVector Forward = GetActorForwardVector();
		Dir = FVector2D(Forward.X, Forward.Y);
		if (Dir.SizeSquared() < 0.001f)
		{
			Dir = FVector2D(1.f, 0.f);
		}
	}
	Dir.Normalize();

	return FVector(FeetXY.X + Dir.X * RequiredHorizSep, FeetXY.Y + Dir.Y * RequiredHorizSep, Feet.Z);
}

void ATankEnemy::SnapshotJumpLandingFloorFromActor_Server(const FVector ProbeLocation, AActor* TraceAlsoIgnoreActor)
{
	FVector FloorHit = ProbeLocation;
	if (!TankJumpTraceLandscapeFloor(GetWorld(), this, TraceAlsoIgnoreActor, ProbeLocation, FloorHit))
	{
		FloorHit = FVector(ProbeLocation.X, ProbeLocation.Y, ProbeLocation.Z);
	}
	TankJumpLandingFloorWorld = FloorHit;
}

void ATankEnemy::TickTankJumpArc_Server()
{
	if (!HasAuthority() || !bTankJumpAttackActive || !bTankJumpKinematicAscent_Server || bTankJumpLandingDamageCommitted_Server)
	{
		return;
	}
	UWorld* const World = GetWorld();
	if (!World || !GetCapsuleComponent())
	{
		return;
	}

	const float Now = World->GetTimeSeconds();
	const float t = float(Now - TankJumpLiftOffStampServerSecs);

	static constexpr float KAirborneTotal = 18.f / 30.f;
	static constexpr float KAscend = 5.f / 30.f;

	if (t >= KAirborneTotal)
	{
		const FVector LandPt(TankJumpLandingFloorWorld);
		const float HalfH = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		SetActorLocation(FVector(LandPt.X, LandPt.Y, LandPt.Z + HalfH), false, nullptr, ETeleportType::TeleportPhysics);
		return;
	}

	const float u = FMath::Clamp(t / KAirborneTotal, 0.f, 1.f);
	const FVector StartXY(TankJumpArcBeginWorld.X, TankJumpArcBeginWorld.Y, 0.f);
	const FVector LandPt = FVector(TankJumpLandingFloorWorld);
	const FVector EndXY(LandPt.X, LandPt.Y, 0.f);
	const FVector CurXY = FMath::Lerp(StartXY, EndXY, u);

	const float HalfH = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float LandCenterZ = LandPt.Z + HalfH;

	const float Asc = FMath::Max(KAscend, KINDA_SMALL_NUMBER);
	float NewZ = 0.f;
	if (t <= Asc)
	{
		NewZ = FMath::Lerp(
			TankJumpArcBeginWorld.Z,
			TankJumpArcBeginWorld.Z + TankJumpPeakZDeltaCm,
			FMath::Clamp(t / Asc, 0.f, 1.f));
	}
	else
	{
		const float FallT = KAirborneTotal - Asc;
		const float Alpha = FallT > KINDA_SMALL_NUMBER ? FMath::Clamp((t - Asc) / FallT, 0.f, 1.f) : 1.f;
		NewZ = FMath::Lerp(TankJumpArcBeginWorld.Z + TankJumpPeakZDeltaCm, LandCenterZ, Alpha);
	}

	const FVector NewLoc(CurXY.X, CurXY.Y, NewZ);
	SetActorLocation(NewLoc, false, nullptr, ETeleportType::TeleportPhysics);
}

void ATankEnemy::ApplyJumpSlamAndRing_Server(const FVector& LandFloorWorld)
{
	if (!HasAuthority() || !bTankJumpAttackActive)
	{
		return;
	}
	if (bTankJumpLandingDamageCommitted_Server)
	{
		return;
	}
	/** 채널링 플래그가 남았거나 순서 역전된 경우 착지 피해 무시 — 다음 노티·타이머에 맡김 */
	if (bHeartbeatChanneling)
	{
		return;
	}

	UWorld* const World = GetWorld();
	if (!World)
	{
		return;
	}

	bTankJumpLandingDamageCommitted_Server = true;
	bTankJumpKinematicAscent_Server = false;

	World->GetTimerManager().ClearTimer(TankJumpLandingFailsafeTimerHandle);

	const float FloorZ = LandFloorWorld.Z;

	if (UCapsuleComponent* const Cap = GetCapsuleComponent())
	{
		const float HalfH = Cap->GetScaledCapsuleHalfHeight();
		SetActorLocation(FVector(LandFloorWorld.X, LandFloorWorld.Y, LandFloorWorld.Z + HalfH), false, nullptr,
			ETeleportType::TeleportPhysics);
	}

	bTankJumpShowLandingTelegraph = false;

	if (UCharacterMovementComponent* const Move = GetCharacterMovement())
	{
		Move->SetMovementMode(MOVE_Walking);
	}

	AController* const Inst = GetController();

	auto ApplyRadialDamageIgnoringSelf = [&](const float RadiusSq, float DamageAmt, float JumpClearCm)
	{
		if (DamageAmt <= KINDA_SMALL_NUMBER || RadiusSq <= SMALL_NUMBER)
		{
			return;
		}

		TArray<FOverlapResult> Hits;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(TankJumpDamageOverlap), false, this);
		Params.AddIgnoredActor(this);

		const FVector OvCenter(LandFloorWorld.X, LandFloorWorld.Y, LandFloorWorld.Z + 80.f);

		const float OuterR = FMath::Sqrt(RadiusSq);
		if (!World->OverlapMultiByChannel(
				Hits, OvCenter, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(OuterR), Params))
		{
			return;
		}

		TSet<AActor*> DamagedActors;
		for (const FOverlapResult& Ov : Hits)
		{
			AActor* const Act = Ov.GetActor();
			if (!IsValid(Act) || DamagedActors.Contains(Act))
			{
				continue;
			}
			const FVector AL = Act->GetActorLocation();
			const float Dx = AL.X - LandFloorWorld.X;
			const float Dy = AL.Y - LandFloorWorld.Y;
			const float DxySq = Dx * Dx + Dy * Dy;
			if (DxySq > RadiusSq + SMALL_NUMBER)
			{
				continue;
			}
			const float FeetAbove = TankJumpResolveFeetCmAboveLandingZ(Act, FloorZ);
			if (FeetAbove > JumpClearCm)
			{
				continue;
			}
			UGameplayStatics::ApplyDamage(Act, DamageAmt, Inst, this, UTankJumpAttackDamageType::StaticClass());
			DamagedActors.Add(Act);
		}
	};

	if (TankJumpSlamDamage > KINDA_SMALL_NUMBER && TankJumpSlamRadiusCm > KINDA_SMALL_NUMBER)
	{
		ApplyRadialDamageIgnoringSelf(FMath::Square(TankJumpSlamRadiusCm), TankJumpSlamDamage,
			TankJumpSlamVictimMaxFeetCmAboveLanding);
	}

	if (TankJumpRingWaveDamage > KINDA_SMALL_NUMBER && TankJumpRingOuterCm > TankJumpSlamRadiusCm + KINDA_SMALL_NUMBER)
	{
		TArray<FOverlapResult> RingHits;
		FCollisionQueryParams RingParams(SCENE_QUERY_STAT(TankJumpRingOv), false, this);
		RingParams.AddIgnoredActor(this);
		const FVector RC(LandFloorWorld.X, LandFloorWorld.Y, LandFloorWorld.Z + 85.f);

		const float SqOuter = TankJumpRingOuterCm * TankJumpRingOuterCm;
		const float SqInner = FMath::Square(TankJumpSlamRadiusCm + 5.f);

		if (World->OverlapMultiByChannel(RingHits, RC, FQuat::Identity, ECC_Pawn,
				FCollisionShape::MakeSphere(TankJumpRingOuterCm), RingParams))
		{
			TSet<AActor*> Seen;
			for (const FOverlapResult& Ov : RingHits)
			{
				AActor* const Act = Ov.GetActor();
				if (!IsValid(Act) || Seen.Contains(Act))
				{
					continue;
				}
				const FVector AL = Act->GetActorLocation();
				const float Dx = AL.X - LandFloorWorld.X;
				const float Dy = AL.Y - LandFloorWorld.Y;
				const float DxySq = Dx * Dx + Dy * Dy;

				if (DxySq < SqInner || DxySq > SqOuter + SMALL_NUMBER)
				{
					continue;
				}

				const float FeetAbove = TankJumpResolveFeetCmAboveLandingZ(Act, FloorZ);
				if (FeetAbove > TankJumpRingVictimJumpClearCmAboveLanding)
				{
					continue;
				}

				UGameplayStatics::ApplyDamage(
					Act, TankJumpRingWaveDamage, Inst, this, UTankJumpAttackDamageType::StaticClass());
				Seen.Add(Act);
			}
		}
	}

	Multicast_SpawnJumpRingBurst(LandFloorWorld);
}

float ATankEnemy::TankJumpResolveFeetCmAboveLandingZ(AActor const* Victim, float LandingFloorWorldZ)
{
	if (!IsValid(Victim))
	{
		return -BIG_NUMBER;
	}
	FVector Feet = Victim->GetActorLocation();
	if (const UCapsuleComponent* Cap = Victim->FindComponentByClass<UCapsuleComponent>())
	{
		Feet.Z -= Cap->GetScaledCapsuleHalfHeight();
	}
	return Feet.Z - LandingFloorWorldZ;
}

bool ATankEnemy::TankJumpTraceLandscapeFloor(UWorld* Map, AActor* IgnoreActor, AActor* TraceAlsoIgnoreActor,
	const FVector WorldProbePt, FVector& OutFloorImpact)
{
	if (!Map)
	{
		return false;
	}

	const FVector TraceStart(WorldProbePt.X, WorldProbePt.Y, WorldProbePt.Z + 1500.f);
	const FVector TraceEnd(WorldProbePt.X, WorldProbePt.Y, WorldProbePt.Z - 40000.f);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TankJumpGroundTrace), false, IgnoreActor);
	if (IsValid(TraceAlsoIgnoreActor))
	{
		Params.AddIgnoredActor(TraceAlsoIgnoreActor);
	}
	if (Map->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, Params))
	{
		OutFloorImpact = Hit.ImpactPoint;
		return true;
	}
	FHitResult VisHit;
	if (Map->LineTraceSingleByChannel(VisHit, TraceStart, TraceEnd, ECC_Visibility, Params))
	{
		OutFloorImpact = VisHit.ImpactPoint;
		return true;
	}
	return false;
}

void ATankEnemy::UpdateJumpLandingAoEIndicatorVisual()
{
	if (!JumpLandingAoERangeIndicatorMesh)
	{
		return;
	}

	const bool bShouldShow = bShowJumpLandingAoETelegraph && IsValid(JumpLandingAoERangeIndicatorMaterial)
		&& bTankJumpAttackActive && IsAlive() && bTankJumpShowLandingTelegraph && TankJumpSlamRadiusCm > KINDA_SMALL_NUMBER
		&& JumpLandingAoERangeIndicatorBuiltInSphereRadiusUU > KINDA_SMALL_NUMBER;

	if (!bShouldShow)
	{
		JumpLandingAoERangeIndicatorMesh->SetVisibility(false);
		JumpLandingAoERangeIndicatorMesh->SetHiddenInGame(true);
		return;
	}

	const FVector Land = FVector(TankJumpLandingFloorWorld);
	JumpLandingAoERangeIndicatorMesh->SetHiddenInGame(false);
	JumpLandingAoERangeIndicatorMesh->SetVisibility(true);
	JumpLandingAoERangeIndicatorMesh->SetWorldLocation(FVector(Land.X, Land.Y, Land.Z + 3.f));
	JumpLandingAoERangeIndicatorMesh->SetWorldRotation(FRotator(0.f, 0.f, 0.f));

	const float XYScale = TankJumpSlamRadiusCm / JumpLandingAoERangeIndicatorBuiltInSphereRadiusUU;
	const float ZScale =
		FMath::Clamp(JumpLandingAoERangeIndicatorDiskThicknessScale, 0.001f, 2.f);

	JumpLandingAoERangeIndicatorMesh->SetWorldScale3D(FVector(XYScale, XYScale, ZScale));
}

void ATankEnemy::Multicast_PlayTankJumpMontage_Implementation()
{
	SyncIdleChaseLocomotionAmbientToFsm(EEnemyAIState::JumpAttack);

	if (!IsValid(TankJumpAttackMontage) || !GetMesh())
	{
		return;
	}
	if (UAnimInstance* const AI = GetMesh()->GetAnimInstance())
	{
		/** 심장박동 몽타주 등이 같은 스켈에서 돌며 점프 착지 노티·타임라인과 겹치는 것 방지 */
		AI->StopAllMontages(0.12f);
		AI->Montage_Play(TankJumpAttackMontage, 1.f, EMontagePlayReturnType::MontageLength, 0.f, true);
	}
}

void ATankEnemy::Multicast_SpawnJumpRingBurst_Implementation(FVector BurstLocationFloor)
{
	if (!IsValid(TankJumpRingNiagaraSystem))
	{
		return;
	}
	if (UWorld* const W = GetWorld())
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(W, TankJumpRingNiagaraSystem, BurstLocationFloor,
			FRotator::ZeroRotator, FVector::OneVector, true);
	}
}
