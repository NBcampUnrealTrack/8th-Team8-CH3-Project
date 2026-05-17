//AFlareWeapon.h
#pragma once

#include "CoreMinimal.h"
#include "Weapon/WeaponBase.h"
#include "FlareWeapon.generated.h"

class AOblivioProjectile;
UCLASS()
class OBLIVIO_API AFlareWeapon : public AWeaponBase
{
	GENERATED_BODY()
public:
	virtual bool UseWeapon() override;
	virtual void ExecuteWeaponAttack(FVector TargetLocation) override;
protected:
	virtual void BeginPlay() override;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Weapon)
	TSubclassOf<AOblivioProjectile> FlareProjectile;
};
