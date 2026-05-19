#include "AIEnemy/HeadlessLoverEnemy.h"

#include "OblivioCharacter.h"
#include "OblivioComponents/EnemyCombatComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

// =============================================================================
// AHeadlessLoverEnemy 구현
// BeginPlay : BlackoutCooldown 주기 타이머 시작
// Die       : ULightDamageType 만 사망 차단(체력 1). 그 외 → Super::Die()
// OnLightHit: HP/사망 없이 1초 경직만
// PerformAttack 베이스는 빈 구현 → 근공격은 Anim Notify 에서 CommitAttackFromAnimNotify.
// TriggerBlackoutPulse: 타이머 콜백 — 범위 무관하게 플레이어 후레시 강제 OFF
// =============================================================================

AHeadlessLoverEnemy::AHeadlessLoverEnemy()
{
	MaxHealth      = 80.0f;
	CurrentHealth  = MaxHealth;
	AttackDamage   = 0.0f;     // HP 피해 없음, 이동 반전만
	AttackRange    = 180.0f;
	AttackCooldown = 3.0f;
	AggroRadius    = 1500.0f;
	bAggroUseHorizontalDistance = true;
	bEnableIdleWander = true;

	// 빛 피격 경직을 1초로 고정
	if (CombatComp)
	{
		CombatComp->StunDuration = 1.0f;
	}
}

void AHeadlessLoverEnemy::BeginPlay()
{
	Super::BeginPlay();

	// BlackoutCooldown(기본 3분) 주기로 암전 펄스 반복 호출.
	// FirstDelay = BlackoutCooldown 이므로 스폰 직후 즉시 발동되지 않는다.
	GetWorldTimerManager().SetTimer(
		BlackoutPulseTimer,
		this,
		&AHeadlessLoverEnemy::TriggerBlackoutPulse,
		BlackoutCooldown,
		/*bLoop=*/true,
		/*FirstDelay=*/BlackoutCooldown);
}

void AHeadlessLoverEnemy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(BlackoutPulseTimer);
	Super::EndPlay(EndPlayReason);
}

// 빛 피해로 Die() 가 호출된 경우만 사망 차단. 경직은 EnemyBase::OnLightHit.
void AHeadlessLoverEnemy::OnLightHit(float Intensity, float Duration)
{
	Super::OnLightHit(Intensity, Duration);
}

void AHeadlessLoverEnemy::Die()
{
	if (WasLastDamageFromLight())
	{
		CurrentHealth = FMath::Max(CurrentHealth, 1.0f);
		return;
	}
	Super::Die();
}

// 근공격 공포 효과는 Anim Notify 의 CommitAttack 경로와 동기화.
// AttackDamage = 0 이므로 레지스트리 통해 HP 피해는 들어오지 않는다.
void AHeadlessLoverEnemy::CommitAttackFromAnimNotify(AActor* OptionalTargetOverride)
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

	if (AOblivioCharacter* Player = Cast<AOblivioCharacter>(HitTarget))
	{
		Player->ApplyMovementInversion(MovementInversionDuration);
	}

	DispatchEnemyAttackCommitted(HitTarget);
}

// 3분 타이머 콜백 — 플레이어 거리/범위 무관하게 후레시를 BlackoutDuration초 강제 OFF.
void AHeadlessLoverEnemy::TriggerBlackoutPulse()
{
	if (!IsAlive()) return;

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (AOblivioCharacter* Player = Cast<AOblivioCharacter>(PlayerPawn))
	{
		Player->ApplyFlashlightBlackout(BlackoutDuration);
	}
}
