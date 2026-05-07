//ThrowableWeapon.h

#pragma once

#include "CoreMinimal.h"
#include "Weapon/WeaponBase.h"
#include "ThrowableWeapon.generated.h"

/**
 * 포물선 투척 무기 베이스.
 * 착지 시 SoundPropagationComponent 를 통해 주변 에너미에게 자극 전파.
 */
UCLASS()
class OBLIVIO_API AThrowableWeapon : public AWeaponBase
{
	GENERATED_BODY()
public:
	void StartThrow(FVector Destination);

protected:
	AThrowableWeapon();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** 착지 시 호출. 파생 클래스에서 추가 로직(폭발, 빛 등)을 override. */
	virtual void OnLanded();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sound")
	class USoundPropagationComponent* SoundPropagationComp;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	float HeightPerDistance;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	float SecondsPerDistance;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	float ThrowOffset;

	bool bIsFlying;
	FVector StartLocation;
	FVector TargetLocation;
	float TimeElapsed;
	float ThrowDuration;
	float ThrowHeight;
};
