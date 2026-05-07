#include "OblivioComponents/EnemyCombatComponent.h"
#include "AIEnemy/EnemyBase.h"
#include "Kismet/GameplayStatics.h"
#include "OblivioComponents/CombatInterface.h"

UEnemyCombatComponent::UEnemyCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UEnemyCombatComponent::BeginPlay()
{
	Super::BeginPlay();
	//이벤트 구독 추가
	if (AEnemyBase* OwnerEnemy = Cast<AEnemyBase>(GetOwner()))
	{
		OwnerEnemy->OnEnemyDamaged.AddDynamic(this, &UEnemyCombatComponent::HandleOwnerDamaged);
		//공격은 레지스트리 전파 사용으로 대체, Enemybase에서 바로 reg호출하기에 필요없음.
		//OwnerEnemy->OnEnemyAttackCommitted.AddDynamic(this, &UEnemyCombatComponent::HandleOwnerAttack);
	}
}

//적 클래스 피격 로직
void UEnemyCombatComponent::HandleOwnerDamaged(float DamageAmount, float CurrentHealth, float MaxHealth)
{
	ICombatInterface* CombatOwner = Cast<ICombatInterface>(GetOwner());
	if (!CombatOwner) return;
	UE_LOG(LogTemp, Warning, TEXT("HandleOwnerDamaged() applies %f damage to %s"), DamageAmount, *GetOwner()->GetName())
	if (DamageAmount >= DamageThreshold)//데미지 기준치 이상일때 데미지 적용 및 스턴
	{
		UE_LOG(LogTemp, Warning, TEXT("Enemy damage applied and stunned"));
		CombatOwner->ApplyHealth(DamageAmount);
		if (!CombatOwner->IsAlive()) {
			//사망 처리 호출
			UE_LOG(LogTemp, Warning, TEXT("Enemy Died!"));
		}
		else {
			CombatOwner->ApplyCCStun(StunDuration);
		}
	}
	else//데미지 기준치 이하일때 슬로우만 적용
	{
		UE_LOG(LogTemp, Log, TEXT("Enemy slowed"));
		CombatOwner->ApplyCCSlow(SlowMultiplier, SlowDuration);
	}
}

/*
//적 클래스 공격 로직
void UEnemyCombatComponent::HandleOwnerAttack(AEnemyBase* Enemy, AActor* Target, float DamageAmount)
{
	if (!Target) return;
	UE_LOG(LogTemp, Log, TEXT("EnemyCombatComponent HandleOwnerAttack Called"));


	//ApplyDamage호출 대신 레지스트리를 통한 델리게이트 전파로 변경
	//UGameplayStatics::ApplyDamage(Target, DamageAmount, nullptr, Enemy, UDamageType::StaticClass()); 
}*/
