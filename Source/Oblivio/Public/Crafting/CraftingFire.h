#pragma once

#include "CoreMinimal.h"
#include "ObstacleBase.h"
#include "CraftingFire.generated.h"

UCLASS()
class OBLIVIO_API ACraftingFire : public AObstacleBase
{
	GENERATED_BODY()
	
public:
    ACraftingFire();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    /** 주변을 밝힐 라이트 컴포넌트 */
    UPROPERTY(VisibleAnywhere, Category = "Components")
    class UPointLightComponent* FireLight;

    /** 몬스터를 감지할 콜리전 영역 */
    UPROPERTY(VisibleAnywhere, Category = "Components")
    class USphereComponent* EffectRange;

    /** 모닥불 속성 */
    UPROPERTY(EditDefaultsOnly, Category = "Fire | Stats")
    float LightIntensity = 5000.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Fire | Stats")
    float BurnDuration = 500.0f; // 배터리 1회 소모당 유지 시간

private:
    float RemainingTime;
    bool bIsActive;

public:
    /** 설치 완료 시 호출 */
    virtual void OnPlaced() override;

    /** 주변 몬스터에게 디버프 적용 */
    void ApplyFireEffect();
};
