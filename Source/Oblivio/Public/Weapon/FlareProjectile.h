//FlareProjectile.h

#pragma once

#include "CoreMinimal.h"
#include "Weapon/OblivioProjectile.h"
#include "FlareProjectile.generated.h"

class ULightAttackComponent;
UCLASS()
class OBLIVIO_API AFlareProjectile : public AOblivioProjectile
{
	GENERATED_BODY()
public:
	virtual void UseWeapon();
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Weapon)
	TObjectPtr<ULightAttackComponent> LightAttackComp;

	AFlareProjectile();
	virtual void StopWeapon();
	virtual void BeginPlay() override;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Weapon)
	float AttackInterval;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Weapon)
	float LastDuration;
	FTimerHandle AttackTimerHandle;
	FTimerHandle DestroyTimerHandle;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
