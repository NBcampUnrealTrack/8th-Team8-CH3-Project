// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BossFloorEffect.generated.h"

class ALuxeaterEnemy;
UCLASS()
class OBLIVIO_API ABossFloorEffect : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ABossFloorEffect();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;


	//컴포넌트
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = BossEffect)
	TObjectPtr<UStaticMeshComponent> MeshComp;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = BossEffect)
	TObjectPtr<UMaterialInstance> FloorMaterial;

	


	//이펙트 변화
	UPROPERTY()
	UMaterialInstanceDynamic* FloorMID;
	UFUNCTION()
	void StartEffect(ALuxeaterEnemy* Enemy);

	UFUNCTION()
	void ClearEffect(AActor* EndPlayActor, const EEndPlayReason::Type EndPlayReason);

	void UpdateMaterial();
	FTimerHandle MaterialUpdateTimerHandle;
	void FinishUpdate();
	FTimerHandle UpdateEndTimerHandle;
	float CurrentAlpha;
	float DeltaAlpha;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = BossEffect)
	float ChangeDuration = 2.f;

};
