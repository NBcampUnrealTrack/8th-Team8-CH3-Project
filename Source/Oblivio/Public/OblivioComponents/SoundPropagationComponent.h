//SoundPropagationComponent.h
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AIEnemy/EnemyBase.h"
#include "SoundPropagationComponent.generated.h"

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class OBLIVIO_API USoundPropagationComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	USoundPropagationComponent();

	UFUNCTION(BlueprintCallable, Category = "Sound")
	void PropagateSound(float VolumeMultiplier = 1.0f);

	UPROPERTY(EditAnywhere, Category = "Sound")
	float Radius;
	UPROPERTY(EditAnywhere, Category = "Sound")
	EEnemyStimulusType StimulusType;

	UPROPERTY(EditAnywhere, Category = "Sound")
	bool bShowDebugSphere = true;
};
