// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Weapon/WeaponBase.h"
#include "FlareGun.generated.h"

class AFlare;
UCLASS()
class OBLIVIO_API AFlareGun : public AWeaponBase
{
	GENERATED_BODY()
public:
	virtual void UseWeapon() override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Weapon)
	TSubclassOf<AFlare> FlareBullet;	//조명탄 탄환
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Weapon)
	float ShotCooldown;	//사격 쿨다운

	FTimerHandle CooldownTimerHandle; //사격 쿨다운 핸들

	
	void ShotFlare(FVector Destination);


};
