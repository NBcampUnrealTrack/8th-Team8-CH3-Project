#pragma once

#include "CoreMinimal.h"
#include "Crafting/ObstacleBase.h"
#include "CraftingReflector.generated.h"

UCLASS()
class OBLIVIO_API ACraftingReflector : public AObstacleBase
{
	GENERATED_BODY()
	
public:
	ACraftingReflector();

protected:
	virtual void BeginPlay() override;

public:
	virtual void OnPlaced() override;
	virtual void Tick(float DeltaTime) override;

protected:
	// 반사되는 빛을 시뮬레이션하기 위한 SpotLight 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class USpotLightComponent* ReflectedLight;

	// 반사판의 반사 효율 (빛의 세기)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reflector")
	float ReflectionIntensity;

	// 반사 각도 (포커스)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reflector")
	float ReflectionConeAngle;
};
