// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Weapon/ThrowableWeapon.h"
#include "Flare.generated.h"

/**
 * 
 */
UCLASS()
class OBLIVIO_API AFlare : public AThrowableWeapon
{
	GENERATED_BODY()
public:
	virtual void UseWeapon() override;
protected:
	AFlare();
	virtual void StopWeapon() override;
	virtual void BeginPlay() override;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Weapon)
	float AttackInterval;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Weapon)
	float LastDuration;
	FTimerHandle AttackTimerHandle;
	FTimerHandle DestroyTimerHandle;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
