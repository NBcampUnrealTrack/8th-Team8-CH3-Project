#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ObstacleBase.generated.h"

/** 장애물의 현재 상태를 정의 */
UENUM(BlueprintType)
enum class EObstacleState : uint8
{
	Ghost,      // 설치 위치 확인
	Placed,     // 설치 완료
	Destroyed   // 파괴됨
};

UCLASS()
class OBLIVIO_API AObstacleBase : public AActor
{
	GENERATED_BODY()
	
public:	
	AObstacleBase();

protected:
	virtual void BeginPlay() override;
    /** 시각적 형상을 담당하는 메쉬 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class UStaticMeshComponent* MeshComponent;

    /** 장애물 기본 속성 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Obstacle | Stats")
    float MaxHealth = 100.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Obstacle | Stats")
    float CurrentHealth;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Obstacle | Cost")
    int32 WoodCost = 0;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Obstacle | Cost")
    int32 IronCost = 0;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Obstacle | Cost")
    float BatteryCostPercent = 0.0f; // 모닥불 전용

    EObstacleState CurrentState = EObstacleState::Ghost;
    bool bIsPlaced = false;
    /** 고스트 모드일 때 적용할 반투명 머티리얼 */
    UPROPERTY(EditDefaultsOnly, Category = "Obstacle | Visual")
    class UMaterialInterface* GhostMaterial;

    /** 실제 설치 완료 시 적용할 원래 머티리얼 */
    UPROPERTY()
    class UMaterialInterface* OriginalMaterial;

public:
    /** 설치를 확정할 때 호출됩니다 */
    virtual void OnPlaced();

    /** 고스트 모드 설정 (색상 피드백 포함) */
    void SetGhostMode(bool bIsGhost, bool bCanPlace = true);

    /** 데미지 처리 (몬스터 공격 대응) */
    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

    // Getter
    int32 GetWoodCost() const { return WoodCost; }
    int32 GetIronCost() const { return IronCost; }
    float GetBatteryCost() const { return BatteryCostPercent; }
    float GetCurrentHealth() const { return CurrentHealth; }

};
