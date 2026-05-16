#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/DamageEvents.h"
#include "TankPlacentaShellActor.generated.h"

class AController;
class ATankEnemy;
class USphereComponent;
class UStaticMeshComponent;

/**
 * 태반방어 패턴 보호구 — 플레이어가 깨도록 대상 가능한 구체 체력.
 * 라이트 공격(LineTrace ECC_Visibility)에 맞으면 탄크 본체에 데미지가 가지 않고 이 액터에만 적중되도록 Sphere 가 시야 채널을 막음.
 *
 * 블루프린트: Content/Enemy/Tank/ 로 `BP_TankPlacentaShell` 을 만들고(ATankPlacentaShellActor 파생),
 * Tank BP 의 `Tank Placenta Shell Class` 에 지정. 지정 안 하면 C++ 디폴트 클래스 스폰.
 */
UCLASS(Blueprintable)
class OBLIVIO_API ATankPlacentaShellActor : public AActor
{
	GENERATED_BODY()

public:
	ATankPlacentaShellActor();

	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator,
		AActor* DamageCauser) override;

	UFUNCTION(BlueprintPure, Category = "Tank|Placenta")
	float GetTankPlacentaShellHealthPercent() const;

	UFUNCTION(BlueprintPure, Category = "Tank|Placenta")
	float GetTankPlacentaShellCurrentHealth() const { return ShellCurrentHealth; }

	bool IsTankPlacentaShellAlive() const { return ShellCurrentHealth > KINDA_SMALL_NUMBER; }

	/** 소유 탱커에 부착, 충돌 반경 설정, 체력 초기화. 서버만 호출. */
	void BindToTank_Server(ATankEnemy* OwnerTank, float InRadiusCm, float InMaxHealth);

	/** 탱커가 패턴 종료 시 남아 있으면 강제 정리할 때 호출. */
	void UnbindAndDestroy_Server();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Placenta")
	TObjectPtr<USphereComponent> CollisionSphere;

	/** 비어 있지 않으면 슬롯 0 에 시각용 메시(구체 등). CollisionSphere 에 Attach. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tank|Placenta|Visual")
	TObjectPtr<UStaticMeshComponent> VisualMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tank|Placenta")
	float ShellMaxHealth = 500.f;

	float ShellCurrentHealth = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tank|Placenta", meta = (ClampMin = "10.0"))
	float ShellSphereRadiusCm = 220.f;

	TWeakObjectPtr<ATankEnemy> TankOwner;

	void BreakShell_Server(AController* EventInstigator, AActor* DamageCauser);

	UFUNCTION(BlueprintNativeEvent, Category = "Tank|Placenta")
	void OnTankPlacentaShellDamaged(float DamageAmount, float ShellHpAfter);

	UFUNCTION(BlueprintNativeEvent, Category = "Tank|Placenta")
	void OnTankPlacentaShellBroken();
};
