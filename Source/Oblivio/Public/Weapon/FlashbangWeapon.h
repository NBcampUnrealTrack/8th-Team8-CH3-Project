//FlashbangWeapon.h

#pragma once

#include "CoreMinimal.h"
#include "Weapon/WeaponBase.h"
#include "FlashbangWeapon.generated.h"

class AOblivioProjectile;
UCLASS()
class OBLIVIO_API AFlashbangWeapon : public AWeaponBase
{
	GENERATED_BODY()
public:
	virtual void BeginPlay() override;
	virtual bool UseWeapon() override;
	virtual void ExecuteWeaponAttack(FVector TargetLocation) override;
protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	TSubclassOf<AOblivioProjectile> FlashbangProjectile;
};
