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

	//플레이어의 피격 처리
	UFUNCTION()
	void HandleOwnerDamaged(AEnemyBase* Enemy, AActor* Target, float DamageAmount);
};
