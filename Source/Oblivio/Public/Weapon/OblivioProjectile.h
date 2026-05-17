// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OblivioProjectile.generated.h"

class USphereComponent;
class UProjectileMovementComponent;
class USoundPropagationComponent;
UCLASS()
class OBLIVIO_API AOblivioProjectile : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AOblivioProjectile();

	UFUNCTION()
	virtual void ThrowProjectile(FVector TargetLocation);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	//투사체
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Projectile)
	TObjectPtr<USphereComponent> SphereComp;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Projectile)
	TObjectPtr<UStaticMeshComponent> MeshComp;

	//포물선
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Projectile)
	TObjectPtr<UProjectileMovementComponent> ProjectileComp;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Projectile)
	float SpeedPerDistance;

	//수명
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Projectile)
	float MaxLifespan;
	FTimerHandle LifeTimerHandle;

	//사운드
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Sound)
	TObjectPtr<UAudioComponent> ReflectionSoundComp;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Sound)
	TObjectPtr<UAudioComponent> TriggerSoundComp;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Sound)
	TObjectPtr<USoundPropagationComponent> SoundPropagationComp;

	//충돌회전효과
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Projectile)
	float ImpactRotationRange = 150.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Projectile)
	float ImpactRotationInterpRate = 3.f;
	FVector SpinVelocity;
	
	UFUNCTION()
	virtual void PlayReflectionSound(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
	UFUNCTION()
	virtual void PlayTriggerSound();
	UFUNCTION()
	virtual void DestroyProjectile();

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
