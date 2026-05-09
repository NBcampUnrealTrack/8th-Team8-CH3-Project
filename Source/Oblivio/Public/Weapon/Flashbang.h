// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Weapon/ThrowableWeapon.h"
#include "Flashbang.generated.h"

/**
 * 
 */
UCLASS()
class OBLIVIO_API AFlashbang : public AThrowableWeapon
{
	GENERATED_BODY()
public:
	virtual void UseWeapon() override;
protected:
	AFlashbang();
	virtual void StopWeapon() override;
	virtual void BeginPlay() override;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Weapon)
	float BangDelay;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Weapon)
	float FlashDuration;
	FTimerHandle BangTimerHandle;
	FTimerHandle DestroyTimerHandle;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
