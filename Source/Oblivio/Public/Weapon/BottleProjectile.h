// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Weapon/OblivioProjectile.h"
#include "BottleProjectile.generated.h"

class UNiagaraSystem;
UCLASS()
class OBLIVIO_API ABottleProjectile : public AOblivioProjectile
{
	GENERATED_BODY()
public:

protected:
	ABottleProjectile();
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	USoundBase* ExplosionSound;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	UNiagaraSystem* ExplosionParticle;

	virtual void PlayReflectionSound(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit) override;
};
