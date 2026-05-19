//BottleWeapon.h

#pragma once

#include "CoreMinimal.h"
#include "Weapon/WeaponBase.h"
#include "BottleWeapon.generated.h"

class AOblivioProjectile;
UCLASS()
class OBLIVIO_API ABottleWeapon : public AWeaponBase
{
	GENERATED_BODY()
public:
	virtual bool UseWeapon() override;
	virtual void ExecuteWeaponAttack(FVector TargetLocation) override;
protected:
	virtual void BeginPlay() override;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Weapon)
	TSubclassOf<AOblivioProjectile> BottleProjectile;
};
