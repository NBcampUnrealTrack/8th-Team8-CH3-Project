#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EnemyCombatComponent.generated.h"

/**
 * 적 캐릭터 전용 전투 컴포넌트입니다.
 * 데미지 수치에 따라 슬로우 또는 데미지+경직을 결정하는 핵심 로직을 포함합니다.
 */
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class OBLIVIO_API UEnemyCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UEnemyCombatComponent();

protected:
	virtual void BeginPlay() override;

	//피격 로직 함수
	UFUNCTION()
	void HandleOwnerDamaged(float DamageAmount, float CurrentHealth, float MaxHealth);

	//공격 로직 함수
	UFUNCTION()
	void HandleOwnerAttack(class AEnemyBase* Enemy, AActor* Target, float DamageAmount);

public:
	//데미지 기준치
	UPROPERTY(EditAnywhere, Category = "Combat Settings")
	float DamageThreshold = 3.5f;

	//슬로우 배율
	UPROPERTY(EditAnywhere, Category = "Combat Settings|Slow")
	float SlowMultiplier = 0.4f;

	//슬로우 시간
	UPROPERTY(EditAnywhere, Category = "Combat Settings|Slow")
	float SlowDuration = 3.0f;

	//경직 시간
	UPROPERTY(EditAnywhere, Category = "Combat Settings|Stun")
	float StunDuration = 1.5f;
};
