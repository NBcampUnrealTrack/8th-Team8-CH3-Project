#include "AIEnemy/WhisperEnemy.h"

#include "AIController.h"
#include "Components/SpotLightComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
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
		if (AWeaponBase* const W = Player->CurrentWeapon.Get())
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
	AttackRange = 150.0f;
	AttackCooldown = 0.2f;
	ChaseAcceptanceRadius = 10.0f;
	ChaseProximityBuffer = 40.0f;

	// Whisper는 거리/배회에 묶이지 않고 계속 접근한다.
	AggroRadius = 0.0f;
	bAggroUseHorizontalDistance = true;
	bEnableIdleWander = false;
	bEnableLightTracking = false;
}

void AWhisperEnemy::BeginPlay()
{
	// BP에 예전 Luxeater 값이 저장돼 있어도 Whisper 컨셉을 강제한다.
	AggroRadius = 0.0f;
	bEnableIdleWander = false;
	bEnableLightTracking = false;
	WhisperRange = FMath::Max(WhisperRange, 150.0f);
	AttackRange = FMath::Max(AttackRange, WhisperRange);

	Super::BeginPlay();
}

void AWhisperEnemy::ApplyCCStun(float /*Duration*/)
{
	// 빛/전투에서 오는 경직 무시 — 둔화(슬로우)만 허용. 회피는 UpdateChase/UpdateAttack.
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

	if (IsWithinWhisperRange())
	{
		TryCommitWhisperAttack();
	}

	ApproachTarget(AI);
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

	if (IsWithinWhisperRange())
	{
		TryCommitWhisperAttack();
	}

	// 베이스 Attack은 StopEnemyMovement를 호출하므로 쓰지 않는다.
	// Whisper는 붙은 뒤에도 계속 압박해야 한다.
	ApproachTarget(AI);
}

bool AWhisperEnemy::IsWithinWhisperRange() const
{
	return IsValid(TargetActor) &&
		FVector::DistSquared2D(GetActorLocation(), TargetActor->GetActorLocation()) <= FMath::Square(WhisperRange);
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
	const bool bResult = bDistOk && bAngleOk;

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

void AWhisperEnemy::ApproachTarget(AAIController* AI)
{
	if (!AI || !IsValid(TargetActor))
	{
		return;
	}

	// 캡슐 겹침으로 일찍 멈추지 않게 StopOnOverlap을 끈다.
	AI->MoveToActor(TargetActor, 5.0f, false);
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

void AWhisperEnemy::TryCommitWhisperAttack()
{
	if (!IsValid(TargetActor))
	{
		return;
	}

	const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (CurrentTime < NextAttackDecisionTime)
	{
		return;
	}

	NextAttackDecisionTime = CurrentTime + AttackCooldown;

	// Whisper는 공격 타이밍/근접 판단만 발행한다.
	// 실제 데미지·연출·손전등 OFF 등 결과는 OnEnemyAttackCommitted/OnAnyEnemyAttackCommitted를 받는
	// 전투 시스템에서 처리한다(캐릭터 변수를 적이 직접 만지지 않음).
	PerformAttack(TargetActor);
	UE_LOG(LogTemp, Verbose, TEXT("Whisper %s committed attack decision"), *GetNameSafe(this));
}
