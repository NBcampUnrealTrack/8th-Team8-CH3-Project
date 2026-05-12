#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LuxeaterLaserMeshProbeActor.generated.h"

class ALuxeaterEnemy;
class UNiagaraSystem;
class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

/**
 * 보스 레이저: 투명 메시(+스피어 충돌) 프로젝타일을 날린 뒤,
 * 표면 충돌 지점에서 나이아가라 재생 후 럭스 이터 데미지 규칙 적용.
 * 비정상 무시 폴백 시 보스 쪽 라인 트레이스 경로 실행.
 */
UCLASS(Blueprintable)
class OBLIVIO_API ALuxeaterLaserMeshProbeActor : public AActor
{
	GENERATED_BODY()

public:
	ALuxeaterLaserMeshProbeActor();

	void ArmProbe(ALuxeaterEnemy* OwningBoss, FVector const& StartWorld,
		FVector const& AimPointWorld, float ProjectileSpeedUU,
		TObjectPtr<UNiagaraSystem> const ImpactFx, UNiagaraSystem* BossFallbackImpactFx,
		float SphereRadiusUU, float LifetimePadSeconds);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleProjectileStopped(FHitResult const& ImpactResult);

	UFUNCTION()
	void OnFailsafeTimeout();

	void ConsumeHit(FHitResult const& Hit);

	UPROPERTY()
	TWeakObjectPtr<ALuxeaterEnemy> CachedBoss;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Probe")
	float FallbackLifePadSeconds = 0.25f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Probe")
	TObjectPtr<USphereComponent> CollisionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Probe")
	TObjectPtr<UStaticMeshComponent> TranslucentVisualMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Probe")
	TObjectPtr<UProjectileMovementComponent> ProjectileMove;

	UPROPERTY()
	TObjectPtr<UNiagaraSystem> ImpactFxSystem;

	UPROPERTY()
	TObjectPtr<UNiagaraSystem> FallbackFxSystemFromBoss;

	UPROPERTY()
	FVector OriginCachedWorld = FVector::ZeroVector;

	UPROPERTY()
	FVector AimPointCached = FVector::ZeroVector;

	FTimerHandle FailsafeTimerHandle;

	bool bHitConsumed = false;
};
