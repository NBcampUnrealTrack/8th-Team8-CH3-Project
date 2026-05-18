#include "AIEnemy/WhisperEnemy.h"

#include "AIController.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SpotLightComponent.h"
#include "CollisionQueryParams.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "WorldCollision.h"
#include "OblivioCharacter.h"
#include "OblivioComponents/LightAttackComponent.h"
#include "Weapon/WeaponBase.h"

namespace
{
	/**
	 * 위스퍼 회피용 콘 정보:
	 * - Origin/Dir 은 캐릭터 FlashlightComponent (캐릭터 회전과 동기화돼 안정적)
	 * - 크기(거리/반각) 는 무기 LightAttackComp (실제 가시 콘과 일치).
	 *   무기가 없으면 FlashlightComponent 의 자체 값으로 폴백.
	 */
	struct FFlashlightCone
	{
		FVector Origin = FVector::ZeroVector;
		FVector Dir = FVector::ForwardVector;
		float MaxDist = 0.f;
		float HalfAngleDeg = 0.f;
		bool bConcentrated = true;
		bool bValid = false;
	};

	bool ResolveFlashlightCone(const AOblivioCharacter* Player, FFlashlightCone& Out)
	{
		if (!Player || !Player->bIsFlashlightOn || Player->Battery <= 0.0f)
		{
			return false;
		}

		const USpotLightComponent* const CSpot = Player->FlashlightComponent;
		if (!CSpot)
		{
			return false;
		}

		Out.Origin = CSpot->GetComponentLocation();
		Out.Dir = CSpot->GetForwardVector().GetSafeNormal();
		if (Out.Dir.IsNearlyZero())
		{
			return false;
		}

		bool bUsedWeapon = false;
		if (AWeaponBase* const W = Player->FlashlightWeapon.Get())
		{
			if (ULightAttackComponent* const LAC = W->FindComponentByClass<ULightAttackComponent>())
			{
				Out.MaxDist = LAC->LightDistance;
				Out.bConcentrated = LAC->bIsConcentrated;
				Out.HalfAngleDeg = LAC->bIsConcentrated ? LAC->LightAngle * 0.5f : 180.0f;
				bUsedWeapon = Out.MaxDist > KINDA_SMALL_NUMBER;
			}
		}
		if (!bUsedWeapon)
		{
			Out.MaxDist = CSpot->AttenuationRadius;
			Out.HalfAngleDeg = CSpot->OuterConeAngle;
			Out.bConcentrated = true;
		}

		Out.bValid = true;
		return true;
	}
}

AWhisperEnemy::AWhisperEnemy()
{
	MaxHealth = 90.0f;
	CurrentHealth = MaxHealth;
	MoveSpeed = 200.0f;
	ChaseMoveSpeed = 200.0f;
	AttackDamage = 0.0f;
	AttackRange = 550.0f;
	AttackCooldown = 0.2f;
	ChaseAcceptanceRadius = 10.0f;
	ChaseProximityBuffer = 40.0f;

	// Whisper는 거리/배회에 묶이지 않고 계속 접근한다.
	AggroRadius = 0.0f;
	bAggroUseHorizontalDistance = true;
	bEnableIdleWander = false;
	bEnableLightTracking = false;

	WhisperAttackAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("WhisperAttackAudio"));
	if (WhisperAttackAudioComponent)
	{
		WhisperAttackAudioComponent->SetupAttachment(RootComponent);
		WhisperAttackAudioComponent->bAutoActivate = false;
		WhisperAttackAudioComponent->bStopWhenOwnerDestroyed = true;
	}
}

void AWhisperEnemy::BeginPlay()
{
	// BP에 예전 Luxeater 값이 저장돼 있어도 Whisper 컨셉을 강제한다.
	AggroRadius = 0.0f;
	bEnableIdleWander = false;
	bEnableLightTracking = false;
	WhisperFightMinDistance = FMath::Max(WhisperFightMinDistance, 200.0f);
	WhisperRange = FMath::Max(WhisperRange, WhisperFightMinDistance + 100.0f);
	AttackRange = FMath::Max(AttackRange, WhisperRange);

	if (WhisperAttackAudioComponent && WhisperAttackSound)
	{
		WhisperAttackAudioComponent->SetSound(WhisperAttackSound);
	}

	Super::BeginPlay();

	ApplyEnemySoundVolumes();
}

void AWhisperEnemy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (WhisperAttackAudioComponent)
	{
		const EAudioComponentPlayState PS = WhisperAttackAudioComponent->GetPlayState();
		if (PS == EAudioComponentPlayState::Playing || PS == EAudioComponentPlayState::FadingIn)
		{
			WhisperAttackAudioComponent->FadeOut(FMath::Max(0.05f, WhisperAttackSoundFadeOutDuration), 0.f,
				EAudioFaderCurve::Linear);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void AWhisperEnemy::ApplyCCStun(float /*Duration*/)
{
	// 빛/전투에서 오는 경직 무시 — 둔화(슬로우)만 허용. 회피는 UpdateChase/UpdateAttack.
}

void AWhisperEnemy::Tick(float DeltaSeconds)
{
	RefreshWalkSpeedFromSources();
	Super::Tick(DeltaSeconds);
	TickWhisperDotDamage(DeltaSeconds);
	TickWhisperAttackLoopAudio();
}

float AWhisperEnemy::GetLocomotionBaseSpeed() const
{
	if (IsSelfInsideFlashlightDanger() && WhisperFlashlightAvoidMoveSpeed > KINDA_SMALL_NUMBER)
	{
		return WhisperFlashlightAvoidMoveSpeed;
	}
	return Super::GetLocomotionBaseSpeed();
}

bool AWhisperEnemy::IsTargetInAttackRange() const
{
	// EnemyBase 기본은 3D AttackRange; 위스퍼 DoT·도넛과 동일한 수평 도넛 + (옵션) 시야로 FSM 갱신.
	return IsWithinWhisperRange() && HasWhisperDotLineOfSightToTarget();
}

bool AWhisperEnemy::IsMeleeCommitNotifyHitValid(AActor const* HitTarget) const
{
	if (!IsValid(HitTarget) || HitTarget != TargetActor)
	{
		return false;
	}
	return IsWithinWhisperRange() && HasWhisperDotLineOfSightToTarget();
}

void AWhisperEnemy::PerformAttack_Implementation(AActor* /*Target*/)
{
	// 근접 피해는 TickWhisperDotDamage에서 초당 WhisperDotDamagePerSecond 로만 처리.
}

void AWhisperEnemy::NotifyWhisperAttackSound_Implementation()
{
	// 속삭임 기본 재생은 TickWhisperAttackLoopAudio 의 AudioComponent 처리. 블루프린트에서 레이어망 추가 가능.
}

void AWhisperEnemy::ApplyEnemySoundVolumes()
{
	Super::ApplyEnemySoundVolumes();

	if (!WhisperAttackAudioComponent)
	{
		return;
	}

	const EAudioComponentPlayState PS = WhisperAttackAudioComponent->GetPlayState();
	if (PS == EAudioComponentPlayState::Playing || PS == EAudioComponentPlayState::FadingIn)
	{
		const float CombinedVol =
			FMath::Max(0.0f, GetEnemySoundVolumeMultiplier()) *
			FMath::Max(0.0f, WhisperAttackSoundVolumeScale);
		WhisperAttackAudioComponent->SetVolumeMultiplier(CombinedVol);
	}
}

void AWhisperEnemy::UpdateChase()
{
	AAIController* const AI = Cast<AAIController>(GetController());
	if (!AI || !IsValid(TargetActor))
	{
		return;
	}

	if (IsSelfInsideFlashlightDanger())
	{
		AvoidFlashlightCone(AI);
		return;
	}

	MaintainEngagementDistance(AI);
}

void AWhisperEnemy::UpdateAttack()
{
	AAIController* const AI = Cast<AAIController>(GetController());
	if (!AI || !IsValid(TargetActor))
	{
		return;
	}

	if (IsSelfInsideFlashlightDanger())
	{
		AvoidFlashlightCone(AI);
		return;
	}

	// 베이스 Attack은 StopEnemyMovement를 호출하므로 쓰지 않는다.
	// 원거리 유지 호흡으로 계속 교전한다.
	MaintainEngagementDistance(AI);
}

bool AWhisperEnemy::IsWithinWhisperRange() const
{
	if (!IsValid(TargetActor))
	{
		return false;
	}

	const float DistSq2D = FVector::DistSquared2D(GetActorLocation(), TargetActor->GetActorLocation());
	const float InnerSq = FMath::Square(WhisperFightMinDistance);
	const float OuterSq = FMath::Square(WhisperRange);
	return DistSq2D <= OuterSq && DistSq2D >= InnerSq;
}

bool AWhisperEnemy::PassesWhisperCombatEngagementBaseline() const
{
	if (!IsAlive() || IsCCStunned() || ShouldSuppressAILocomotion())
	{
		return false;
	}

	if (EnemyState != EEnemyAIState::Chase && EnemyState != EEnemyAIState::Attack)
	{
		return false;
	}

	if (!IsValid(TargetActor))
	{
		return false;
	}

	if (IsSelfInsideFlashlightDanger() || !IsWithinWhisperRange() || !HasWhisperDotLineOfSightToTarget())
	{
		return false;
	}

	return true;
}

bool AWhisperEnemy::DoesGeometryBlockLosBetween(FVector const& TraceStart, FVector const& TraceEnd,
	float const ClearanceCm) const
{
	UWorld const* const World = GetWorld();
	if (!World)
	{
		return false;
	}

	const float DistToGoal = FVector::Dist(TraceStart, TraceEnd);
	if (DistToGoal <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float Margin = FMath::Max(10.f, ClearanceCm);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(WhisperGeomLos), false, this);
	if (IsValid(TargetActor))
	{
		Params.AddIgnoredActor(TargetActor);
	}

	auto const IsBlockedEarlyOnChannel = [&](ECollisionChannel const Channel) -> bool
	{
		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, Channel, Params))
		{
			return false;
		}
		return Hit.Distance < DistToGoal - Margin;
	};

	return IsBlockedEarlyOnChannel(ECC_WorldStatic) || IsBlockedEarlyOnChannel(ECC_Visibility);
}

bool AWhisperEnemy::HasWhisperDotLineOfSightToTarget() const
{
	if (!bRequireLineOfSightForWhisperDot)
	{
		return true;
	}

	UWorld const* const World = GetWorld();
	if (!World || !IsValid(TargetActor))
	{
		return false;
	}

	FVector TraceStart;
	FRotator UnusedRot;
	GetActorEyesViewPoint(TraceStart, UnusedRot);

	FVector TraceEnd = TargetActor->GetActorLocation();
	if (APawn const* const PawnT = Cast<APawn>(TargetActor))
	{
		if (UCapsuleComponent const* const Caps = PawnT->FindComponentByClass<UCapsuleComponent>())
		{
			TraceEnd = Caps->GetComponentLocation();
		}
	}

	return !DoesGeometryBlockLosBetween(TraceStart, TraceEnd, WhisperDotLosClearanceCm);
}

bool AWhisperEnemy::IsPointInsideFlashlightDanger(const FVector& Point) const
{
	const AOblivioCharacter* const Player = Cast<AOblivioCharacter>(TargetActor);
	FFlashlightCone Cone;
	if (!ResolveFlashlightCone(Player, Cone))
	{
#if !UE_BUILD_SHIPPING
		if (bDebugDrawFlashlightDanger && GEngine)
		{
			GEngine->AddOnScreenDebugMessage((uint64)(UPTRINT)this, 0.0f, FColor::Silver,
				FString::Printf(TEXT("Whisper %s: flashlight gate FAIL (Player=%d On=%d Bat=%.1f)"),
					*GetNameSafe(this),
					Player ? 1 : 0,
					Player ? (Player->bIsFlashlightOn ? 1 : 0) : 0,
					Player ? Player->Battery : -1.f));
		}
#endif
		return false;
	}

	const FVector ToPoint = Point - Cone.Origin;
	const float Dist = ToPoint.Size();
	if (Dist <= KINDA_SMALL_NUMBER)
	{
		return true;
	}

	const float MaxDist = Cone.MaxDist + DangerConeRadiusSlack;
	const bool bOmni = Cone.HalfAngleDeg >= 90.f; // 비집중(점광원)일 때

	const float HalfOuterDeg = bOmni ? 90.f : FMath::Min(89.0f, Cone.HalfAngleDeg + DangerConeAngleMarginDeg);
	const float CosCone = FMath::Cos(FMath::DegreesToRadians(HalfOuterDeg));
	const float DotVal = FVector::DotProduct(Cone.Dir, ToPoint / Dist);

	const bool bDistOk = Dist <= MaxDist;
	const bool bAngleOk = bOmni || DotVal >= CosCone - KINDA_SMALL_NUMBER;
	bool bResult = bDistOk && bAngleOk;

	if (bResult && bRequireLineOfSightForFlashlightAvoid)
	{
		if (DoesGeometryBlockLosBetween(Cone.Origin, Point, WhisperDotLosClearanceCm))
		{
			bResult = false;
		}
	}

#if !UE_BUILD_SHIPPING
	if (bDebugDrawFlashlightDanger && GetWorld())
	{
		const float DrawHalfDeg = bOmni ? 89.0f : HalfOuterDeg;
		DrawDebugCone(GetWorld(), Cone.Origin, Cone.Dir, MaxDist,
			FMath::DegreesToRadians(DrawHalfDeg), FMath::DegreesToRadians(DrawHalfDeg),
			16, bResult ? FColor::Red : FColor::Yellow, false, 0.f, 0, 1.5f);
		DrawDebugLine(GetWorld(), Cone.Origin, Point, bResult ? FColor::Red : FColor::Cyan, false, 0.f, 0, 1.5f);

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage((uint64)(UPTRINT)this, 0.0f,
				bResult ? FColor::Red : FColor::Yellow,
				FString::Printf(TEXT("Whisper %s: dist=%.0f/%.0f(%s) halfDeg=%.0f dot=%.2f(%s) → %s"),
					*GetNameSafe(this),
					Dist, MaxDist, bDistOk ? TEXT("OK") : TEXT("FAR"),
					DrawHalfDeg, DotVal, bAngleOk ? TEXT("OK") : TEXT("OUT"),
					bResult ? TEXT("DANGER") : TEXT("safe")));
		}
	}
#endif

	return bResult;
}

bool AWhisperEnemy::IsSelfInsideFlashlightDanger() const
{
	return IsPointInsideFlashlightDanger(GetActorLocation());
}

void AWhisperEnemy::MaintainEngagementDistance(AAIController* AI)
{
	if (!AI || !IsValid(TargetActor))
	{
		return;
	}

	const FVector EnemyLoc = GetActorLocation();
	const FVector TargetLoc = TargetActor->GetActorLocation();
	FVector FromPlayerToEnemy = EnemyLoc - TargetLoc;
	FromPlayerToEnemy.Z = 0.0f;

	const float Dist = FromPlayerToEnemy.Size();
	FVector DirOut = Dist > KINDA_SMALL_NUMBER ? FromPlayerToEnemy / Dist : FVector::ForwardVector;

	const float Span = WhisperRange - WhisperFightMinDistance;
	const float DesiredRadius = WhisperFightMinDistance + FMath::Max(20.0f, Span * 0.5f);

	// 너무 안쪽이면 밖으로, 도트 최대(WhisperRange)보다 멀면 안으로.
	// 예전에는 바깥 판정에 +35 slack 을 두어 (WhisperRange, WhisperRange+35] 에서 멈추며
	// 도넛에 못 들어가는 상태가 발생할 수 있었음 → 바깥은 Dist > WhisperRange 만 사용.
	if (Dist < WhisperFightMinDistance)
	{
		FVector Goal = TargetLoc + DirOut * DesiredRadius;
		Goal.Z = EnemyLoc.Z;
		AI->MoveToLocation(Goal, 48.0f, false);
		return;
	}

	if (Dist > WhisperRange)
	{
		FVector Goal = TargetLoc + DirOut * DesiredRadius;
		Goal.Z = EnemyLoc.Z;
		AI->MoveToLocation(Goal, 48.0f, false);
		return;
	}

	AI->StopMovement();
}

void AWhisperEnemy::AvoidFlashlightCone(AAIController* AI)
{
	const AOblivioCharacter* const Player = Cast<AOblivioCharacter>(TargetActor);
	FFlashlightCone Cone;
	if (!AI || !Player || !ResolveFlashlightCone(Player, Cone))
	{
		return;
	}

	FVector Forward = FVector(Cone.Dir.X, Cone.Dir.Y, 0.0f).GetSafeNormal();
	if (Forward.IsNearlyZero())
	{
		Forward = Player->GetActorForwardVector().GetSafeNormal2D();
	}

	const FVector Origin = Cone.Origin;
	FVector ToEnemy = GetActorLocation() - Origin;
	ToEnemy.Z = 0.0f;

	const FVector Right(-Forward.Y, Forward.X, 0.0f);
	const float Side = FVector::DotProduct(ToEnemy, Right);
	const float Sign = (Side >= 0.0f) ? 1.0f : -1.0f;

	const FVector EscapeGoal = GetActorLocation() + Right * (Sign * 700.0f) - Forward * 150.0f;
	AI->MoveToLocation(EscapeGoal, 10.0f);
}

void AWhisperEnemy::TickWhisperDotDamage(float DeltaSeconds)
{
	if (!PassesWhisperCombatEngagementBaseline() || DeltaSeconds <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	if (WhisperDotDamagePerSecond <= 0.f)
	{
		return;
	}

	const float Amount = WhisperDotDamagePerSecond * DeltaSeconds;
	if (Amount > 0.f)
	{
		UGameplayStatics::ApplyDamage(TargetActor, Amount, nullptr, this, UDamageType::StaticClass());
		NotifyWhisperAttackSound();
	}
}

void AWhisperEnemy::TickWhisperAttackLoopAudio()
{
	if (!WhisperAttackAudioComponent)
	{
		return;
	}

	const bool bWantLoop =
		PassesWhisperCombatEngagementBaseline() &&
		WhisperDotDamagePerSecond > KINDA_SMALL_NUMBER &&
		WhisperAttackSound != nullptr;

	const float CombinedVol = FMath::Max(0.0f, GetEnemySoundVolumeMultiplier()) *
		FMath::Max(0.0f, WhisperAttackSoundVolumeScale);

	if (bWantLoop != bWhisperAttackAudioTrackedOn)
	{
		if (bWantLoop)
		{
			WhisperAttackAudioComponent->Stop();
			WhisperAttackAudioComponent->SetSound(WhisperAttackSound);
			WhisperAttackAudioComponent->SetVolumeMultiplier(CombinedVol);
			WhisperAttackAudioComponent->FadeIn(
				FMath::Max(0.02f, WhisperAttackSoundFadeInDuration),
				1.f,
				0.f,
				EAudioFaderCurve::Linear);
		}
		else
		{
			const EAudioComponentPlayState PS = WhisperAttackAudioComponent->GetPlayState();
			if (PS == EAudioComponentPlayState::Playing || PS == EAudioComponentPlayState::FadingIn)
			{
				WhisperAttackAudioComponent->FadeOut(
					FMath::Max(0.05f, WhisperAttackSoundFadeOutDuration),
					0.f,
					EAudioFaderCurve::Linear);
			}
			else if (PS != EAudioComponentPlayState::FadingOut)
			{
				WhisperAttackAudioComponent->Stop();
			}
		}

		bWhisperAttackAudioTrackedOn = bWantLoop;
	}
	else if (bWantLoop)
	{
		if (WhisperAttackAudioComponent->GetSound() != WhisperAttackSound)
		{
			WhisperAttackAudioComponent->Stop();
			WhisperAttackAudioComponent->SetSound(WhisperAttackSound);
			WhisperAttackAudioComponent->SetVolumeMultiplier(CombinedVol);
			WhisperAttackAudioComponent->FadeIn(
				FMath::Max(0.02f, WhisperAttackSoundFadeInDuration),
				1.f,
				0.f,
				EAudioFaderCurve::Linear);
		}
		else
		{
			WhisperAttackAudioComponent->SetVolumeMultiplier(CombinedVol);
		}
	}
}
