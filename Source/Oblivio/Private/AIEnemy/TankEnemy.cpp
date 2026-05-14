#include "AIEnemy/TankEnemy.h"

#include "Combat/TankHeartbeatDamageType.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Materials/MaterialInterface.h"
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
	// 심장 표시 시점은 TankHeartShow 애님 노티 전용(채널링 시작과 분리)
	SetTankHeartMeshVisible(false);

	if (HeartbeatAoERangeIndicatorMesh && HeartbeatAoERangeIndicatorMaterial)
	{
		HeartbeatAoERangeIndicatorMesh->SetMaterial(0, HeartbeatAoERangeIndicatorMaterial);
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

void ATankEnemy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATankEnemy, bHeartbeatChanneling);
}

bool ATankEnemy::HasValidAggroTarget() const
{
	if (Super::HasValidAggroTarget())
	{
		return true;
	}
	if (!bUseHeartbeatAoEAttack || !IsValid(TargetActor))
	{
		return false;
	}
	return IsTargetInHeartbeatAoERange();
}

EEnemyAIState ATankEnemy::SelectStateWhileAggroed() const
{
	if (bUseHeartbeatAoEAttack && bHeartbeatChanneling && IsAlive())
	{
		return EEnemyAIState::Heartbeat;
	}
	return Super::SelectStateWhileAggroed();
}

bool ATankEnemy::TryConsumeSpecialFSMUpdate()
{
	if (!bUseHeartbeatAoEAttack || !bHeartbeatChanneling || !IsAlive())
	{
		return false;
	}
	// UpdateState 가 bAggro==false 일 때 Idle/Search 로 떨어지며 Chase·Heartbeat 를 덮어쓰는 것을 막음
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
	if (HeartbeatAoERangeIndicatorMesh)
	{
		HeartbeatAoERangeIndicatorMesh->SetVisibility(false);
		HeartbeatAoERangeIndicatorMesh->SetHiddenInGame(true);
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
	bHeartbeatChanneling = false;

	if (HeartbeatAoERangeIndicatorMesh)
	{
		HeartbeatAoERangeIndicatorMesh->SetVisibility(false);
		HeartbeatAoERangeIndicatorMesh->SetHiddenInGame(true);
	}

	Super::EndPlay(EndPlayReason);
}

void ATankEnemy::Tick(float DeltaSeconds)
{
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
	if (!IsValid(TargetActor))
	{
		return false;
	}
	const FVector SelfLoc = GetActorLocation();
	const FVector TargetLoc = TargetActor->GetActorLocation();
	const float SqR = FMath::Square(HeartbeatAoERadius);
	return FVector::DistSquared(SelfLoc, TargetLoc) <= SqR;
}

void ATankEnemy::TryStartHeartbeatSequence()
{
	UWorld* const World = GetWorld();
	if (!World || !HasAuthority())
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
	bHeartbeatChanneling = false;
	// 서버 전용 종료 경로이므로 권한 없는 머신은 OnRep 로 끔
	SetTankHeartMeshVisible(false);
	if (UWorld* const W = GetWorld())
	{
		LastHeartbeatSequenceEndWorldTime = W->GetTimeSeconds();
	}

	if (IsAlive())
	{
		// 내부 EnemyState 가 이미 Chase 면 SetEnemyState(Chase) 가 무시되어 MoveTo·이속이 갱신되지 않을 수 있음
		SetEnemyState(SelectStateWhileAggroed(), true);
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
