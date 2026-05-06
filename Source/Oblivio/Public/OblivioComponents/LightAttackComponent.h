//LightAttackComponent.h

#pragma once

#include "CoreMinimal.h"
#include "LightAttackInterface.h"
#include "Components/SceneComponent.h"
#include "LightAttackComponent.generated.h"

class USpotLightComponent;
class UPointLightComponent;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class OBLIVIO_API ULightAttackComponent : public USceneComponent, public ILightAttackInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	ULightAttackComponent();

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual void CreateLightAttack(FVector SourceLocation, FVector LightDirection) override;

	//실제 광원
	UPROPERTY(VisibleAnywhere, Category = "Light Attack")
	TObjectPtr<USpotLightComponent> SpotLightComp;   // bIsConcentrated
	UPROPERTY(VisibleAnywhere, Category = "Light Attack")
	TObjectPtr<UPointLightComponent> PointLightComp;   // bIsConcentrated
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Light Attack")
	FColor BasicLightColor;   // bIsConcentrated

	//빛 세기/시간
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Light)
	float LightIntensityScale; //how strong the light intensity will be synced with the Damage value;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Light)
	float LightTime;		//how long the lightsource will stay turned on after CreateLightAttack() called
	FTimerHandle LightOffTimerHandle;
	void TurnOffLight();

	//각도 조절
	void ChangeLightAngle(float Angle);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Light)
	float MinAngle;		//How narrowest the light can get
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Light)
	float MaxAngle;		//How broadest the light can get
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Light)
	float DistancePerAngle;		//Distance/angle trade ratio when changing the angle;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Light)
	float DamagePerAngle;		//Damage/angle trade ratio when changing the angle;

	//빛 데미지 판정용
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Light)
	float Damage;		//Damage taken if light is lit
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Light)
	bool bIsConcentrated;	//Where light spread in circular area or in a sector.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Light)
	float LightAngle;		//Angle of sector if light is concentrated
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Light)
	float LightDistance;	//the maximum distance the light reaches
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Light)
	float MaxDamageDistance;	//Where to start taking maximum damage
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Light)
	float DamageAttenuationRate;	//How fast the damage decreases as distance is far from the source.


protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void OnRegister() override;

};
