#include "AIEnemy/HeadlessLoverEnemy.h"

#include "OblivioCharacter.h"
#include "OblivioComponents/EnemyCombatComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

// =============================================================================
// AHeadlessLoverEnemy 구현
// BeginPlay : BlackoutCooldown 주기 타이머 시작
// Die       : no-op (불사) — CurrentHealth를 1로 클램프해 사망 차단
// OnLightHit: HP/사망 없이 1초 경직만
// PerformAttack: 근접 히트 → 플레이어 이동 반전(MovementInversionDuration초)
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

// 빛 피격: HP 피해는 Die() 오버라이드로 이미 차단되어 있으므로
// 경직 누적/발동 로직은 EnemyBase::OnLightHit에 위임한다.
// (LightStunBuildupSeconds, LightStunDuration 모두 베이스에서 처리)
void AHeadlessLoverEnemy::OnLightHit(float Intensity, float Duration)
{
	Super::OnLightHit(Intensity, Duration);
}

// 불사 — HP가 0 이하가 돼도 사망하지 않는다.
// TakeDamage는 기존 경로로 흐르되(CombatComp CC 판정 등), Die 호출 시점에서 차단한다.
void AHeadlessLoverEnemy::Die()
{
	CurrentHealth = FMath::Max(CurrentHealth, 1.0f);
}

// 근접 공격 히트 → 플레이어 이동 방향 반전(MovementInversionDuration초).
// AttackDamage = 0 이므로 EnemyCombatComponent는 HP 피해 0을 전달한다.
void AHeadlessLoverEnemy::PerformAttack_Implementation(AActor* Target)
{
	if (AOblivioCharacter* Player = Cast<AOblivioCharacter>(Target))
	{
		Player->ApplyMovementInversion(MovementInversionDuration);
	}

	OnEnemyAttackCommitted.Broadcast(this, Target, AttackDamage);
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
