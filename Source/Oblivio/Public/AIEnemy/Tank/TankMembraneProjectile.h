#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TankMembraneProjectile.generated.h"

class UNiagaraSystem;
class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

/** 양막 패턴 투사체 — 직선 비행, 서버에서만 플레이어 ApplyHealth. */
UCLASS(Blueprintable)
class OBLIVIO_API ATankMembraneProjectile : public AActor
{
	GENERATED_BODY()

public:
	ATankMembraneProjectile();

	virtual void BeginPlay() override;

	void InitializeFlight(float InDamage, float InSpeedUU, FVector const& DirectionWorld);

	/**
	 * 같은 부채꼴/같은 양막 그룹에서 동시에 스폰된 다른 투사체끼리 충돌·파괴하지 않도록
	 * 양방향 IgnoreActorWhenMoving 을 등록한다. emitter 가 스폰 직후 호출.
	 */
	void IgnoreOtherMembraneProjectile(ATankMembraneProjectile* Other);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Membrane")
	TObjectPtr<USphereComponent> CollisionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Membrane")
	TObjectPtr<UProjectileMovementComponent> ProjectileMove;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Membrane")
	TObjectPtr<UStaticMeshComponent> VisualSphere;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tank|Membrane|VFX")
	TObjectPtr<UNiagaraSystem> TrailNiagaraSystem;

	UPROPERTY(BlueprintReadOnly, Category = "Tank|Membrane")
	float Damage = 8.f;

	/** 스폰 직후 이 시간 동안의 hit 은 무시(스폰 위치가 벽/캡슐과 겹쳐 즉시 자폭하는 문제 방지). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Membrane",
		meta = (ClampMin = "0.0"))
	float SpawnHitGraceSeconds = 0.10f;

	UFUNCTION()
	void OnSphereHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		FVector NormalImpulse, const FHitResult& Hit);

	bool bFlightInitialized = false;
	double SpawnWorldSeconds = 0.0;
};
