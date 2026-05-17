// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Weapon/WeaponBase.h"
#include "Flashlight.generated.h"

/**
 * 
 */
UCLASS()
class OBLIVIO_API AFlashlight : public AWeaponBase
{
	GENERATED_BODY()
public:
	AFlashlight();
	virtual void BeginPlay() override;
	virtual bool UseWeapon() override;
	virtual void StopWeapon() override;
	virtual void ChangeWeaponAngle(float DeltaAngle) override;

	UFUNCTION(BlueprintPure, Category = Attack)
	float GetAttackInterval() const { return AttackInterval; }

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Attack)
	float AttackInterval;
	FTimerHandle AttackTimerHandle;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Weapon)
	TObjectPtr<ULightAttackComponent> LightAttackComp;
};
