#include "OblivioComponents/PlayerCombatComponent.h"
#include "OblivioComponents/CombatInterface.h"
#include "Kismet/GameplayStatics.h"
#include "OblivioCharacter.h"
#include "Combat/EnemyCombatRegistrySubsystem.h"

UPlayerCombatComponent::UPlayerCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPlayerCombatComponent::BeginPlay()
{
	Super::BeginPlay();
	//이벤트 구독 추가
	UEnemyCombatRegistrySubsystem* Reg =
		UEnemyCombatRegistrySubsystem::GetEnemyCombatRegistry(this);
	if (Reg)
	{
		Reg->OnAnyEnemyAttackCommitted.AddDynamic(this, &UPlayerCombatComponent::HandleOwnerDamaged);
	}
}

void UPlayerCombatComponent::HandleOwnerDamaged(AEnemyBase* Enemy, AActor* Target, float DamageAmount)
{
	ICombatInterface* MyOwner = Cast<ICombatInterface>(GetOwner());
	//체력 감소 적용
	if (MyOwner)
	{ 
		MyOwner->ApplyHealth(DamageAmount);
		//CC 호출하고싶으면 추가
	}
}
