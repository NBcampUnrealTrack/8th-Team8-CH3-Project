#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlayerCombatComponent.generated.h"

class AEnemyBase;
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class OBLIVIO_API UPlayerCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UPlayerCombatComponent();

protected:
	virtual void BeginPlay() override;

	//플레이어의 피격 처리(적 공격 커밋 시). DamageTypeClass 는 레지스트리 브로드캐스트용(null 이면 기본 근접).
	UFUNCTION()
	void HandleOwnerDamaged(AEnemyBase* Enemy, AActor* Target, float DamageAmount, UClass* DamageTypeClass);
};
