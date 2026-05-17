// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Weapon/OblivioProjectile.h"
#include "FlashbangProjectile.generated.h"

class ULightAttackComponent;
UCLASS()
class OBLIVIO_API AFlashbangProjectile : public AOblivioProjectile
{
	GENERATED_BODY()
public:
	virtual void UseWeapon();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Weapon)
	TObjectPtr<ULightAttackComponent> LightAttackComp;

	AFlashbangProjectile();
	virtual void StopWeapon();
	virtual void BeginPlay() override;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Weapon)
	float BangDelay;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	USoundBase* ExplosionSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Weapon)
	float FlashDuration;
	FTimerHandle BangTimerHandle;
	FTimerHandle DestroyTimerHandle;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
};
