#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AIEnemy/Tank/TankMembraneAnimTypes.h"
#include "TankMembraneEmitterActor.generated.h"

class ATankEnemy;
class ATankMembraneProjectile;
class UMaterialInstanceDynamic;
class USoundBase;
class UStaticMeshComponent;

/**
 * 양막 소환 지점에서 일정 간격으로 부채꼴(3발) 직선 투사체를 N회 발사하고 자가 파괴.
 *
 * 발사 튜닝(부채각 / 공격 횟수 / 공격 간격 / 투사체 속도)은 양막 BP에서 직접 수정한다.
 * Tank 가 ConfigureAndStartBurst 로 Override 값을 넘기면 그 값이 우선이며, 0/음수면 BP 기본값을 사용.
 */
UCLASS(Blueprintable)
class OBLIVIO_API ATankMembraneEmitterActor : public AActor
{
	GENERATED_BODY()

public:
	ATankMembraneEmitterActor();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 현재 양막 연출 단계(NetMulticast로 동기화). */
	UPROPERTY(BlueprintReadOnly, Category = "Tank|Membrane|Anim")
	ETankMembraneEmitterAnimPhase MembraneAnimPhase = ETankMembraneEmitterAnimPhase::None;

	/** 애님/나이아가라 등 — 단계 변경 시(서버·클라 동시). */
	UFUNCTION(BlueprintNativeEvent, Category = "Tank|Membrane|Anim")
	void OnMembraneEmitterAnimPhaseChanged(ETankMembraneEmitterAnimPhase NewPhase);

	/**
	 * 발사 시작.
	 * In* 인자 중 0/음수는 무시되고 BP에서 설정한 멤버 기본값이 사용된다.
	 * VolleyCount 만큼 VolleyIntervalSeconds 간격으로 부채꼴 3발을 쏜 뒤 자가 파괴한다.
	 */
	void ConfigureAndStartBurst(ATankEnemy* InTank, FTransform const& SpawnWorld,
		float InFanHalfAngleDeg, float InProjectileSpeedUU, float InProjectileDamage,
		int32 InVolleyCount, float InVolleyIntervalSeconds,
		TSubclassOf<ATankMembraneProjectile> InProjectileClass);

protected:
	/** 서버에서만 호출 — NetMulticast로 단계 동기화. */
	void SetMembraneAnimPhase(ETankMembraneEmitterAnimPhase NewPhase);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_SetMembraneAnimPhase(ETankMembraneEmitterAnimPhase NewPhase);

	/** 투사체 1발 발사 SFX 를 모든 머신에서 재생. */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayProjectileFireSfx(FVector const& Location);

	/** 한 볼리 펄스(초록 점등)를 모든 머신에서 트리거. */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayMembraneFirePulse(FVector const& MuzzleLocationWorld);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Membrane")
	TObjectPtr<UStaticMeshComponent> EmitterMesh;

	// ----------------------------- 발사 튜닝 (양막 BP에서 수정) -----------------------------

	/** 부채꼴 절반 각도(deg). 좌(-X) / 중앙 / 우(+X) 3발이 이 각도로 벌어진다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Membrane|Volley",
		meta = (ClampMin = "0.0", ClampMax = "89.0",
			ToolTip = "Tank 가 0보다 큰 값을 넘기지 않으면 이 값이 사용된다."))
	float FanHalfAngleDeg = 12.f;

	/** 한 발 투사체 속도(UU/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Membrane|Volley",
		meta = (ClampMin = "1.0",
			ToolTip = "Tank 가 0보다 큰 값을 넘기지 않으면 이 값이 사용된다."))
	float ProjectileSpeedUU = 2400.f;

	/** 양막 1개가 발사할 부채꼴 볼리 횟수. 다 발사하면 자가 파괴. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Membrane|Volley",
		meta = (ClampMin = "1",
			ToolTip = "Tank 가 0보다 큰 값을 넘기지 않으면 이 값이 사용된다."))
	int32 VolleyCount = 3;

	/** 볼리 사이 간격(초). 작을수록 공격 속도가 빠르다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Membrane|Volley",
		meta = (ClampMin = "0.05",
			ToolTip = "Tank 가 0보다 큰 값을 넘기지 않으면 이 값이 사용된다."))
	float VolleyIntervalSeconds = 0.6f;

	/** 첫 볼리까지 지연(초). 0이면 ConfigureAndStartBurst 와 같은 프레임에 첫 발 발사. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Membrane|Volley",
		meta = (ClampMin = "0.0"))
	float FirstVolleyDelaySeconds = 0.0f;

	// ---------------------------------------------------------------------------------------
	// Muzzle 오프셋 — 투사체 스폰 위치를 양막 액터 원점에서 얼마나 띄울지.
	// 양막 본체가 바닥/벽에 박혀 있어 투사체가 즉시 충돌→정지하는 문제를 막는다.
	// Forward(앞)는 부채꼴 직진 방향(=발사 방향), Up(위)는 월드 +Z 방향.
	// ---------------------------------------------------------------------------------------

	/** 발사 방향(전방)으로 띄울 거리(cm). 부채꼴 3발 각각의 Yaw 방향을 따라 오프셋된다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Membrane|Volley|Muzzle",
		meta = (ClampMin = "0.0"))
	float ProjectileSpawnForwardCm = 35.f;

	/**
	 * 위(World +Z)로 띄울 높이(cm). 양막이 바닥에 놓여 있어 첫 프레임에 Floor 와 충돌→정지하는
	 * 문제를 막는다. 캐릭터 가슴/허리 높이(보통 80~120 cm)가 적당.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Membrane|Volley|Muzzle",
		meta = (ClampMin = "0.0"))
	float ProjectileSpawnUpCm = 90.f;

	// ---------------------------------------------------------------------------------------
	// SFX — 부채꼴 3발 스폰 시 각 발마다 1회씩 PlaySoundAtLocation.
	// ---------------------------------------------------------------------------------------

	/** 투사체 1발 발사 시 재생할 사운드. 비어 있으면 SFX 없음. 부채꼴 3발이면 자동으로 3번 재생됨. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Membrane|Volley|SFX")
	TObjectPtr<USoundBase> ProjectileFireSfx;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Membrane|Volley|SFX",
		meta = (ClampMin = "0.0"))
	float ProjectileFireSfxVolume = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Membrane|Volley|SFX",
		meta = (ClampMin = "0.01"))
	float ProjectileFireSfxPitch = 1.f;

	// ---------------------------------------------------------------------------------------
	// Pulse — 발사 순간 양막 본체가 심장처럼 점등되도록 머티리얼 벡터 파라미터를 set.
	// EmitterMesh 머티리얼에 같은 이름의 Vector Parameter 가 있어야 동작 (예: EmissiveColor).
	// 한 볼리(부채꼴 3발)당 1회 펄스. BP 에서 OnMembraneFirePulse 를 오버라이드해 추가 FX 가능.
	// ---------------------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Membrane|Volley|Pulse")
	FLinearColor PulseColor = FLinearColor(0.f, 4.5f, 0.6f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Membrane|Volley|Pulse")
	FLinearColor PulseRestColor = FLinearColor(0.f, 0.f, 0.f, 1.f);

	/** EmitterMesh 의 머티리얼에서 set 할 Vector Parameter 이름. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Membrane|Volley|Pulse")
	FName PulseColorParameterName = TEXT("EmissiveColor");

	/** EmitterMesh 머티리얼 슬롯 인덱스. 보통 0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Membrane|Volley|Pulse",
		meta = (ClampMin = "0"))
	int32 PulseMaterialElementIndex = 0;

	/** 펄스가 PulseColor → PulseRestColor 로 줄어드는 시간(초). 0이면 펄스 비활성. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Membrane|Volley|Pulse",
		meta = (ClampMin = "0.0"))
	float PulseDecaySeconds = 0.35f;

	/**
	 * 한 볼리(부채꼴 3발) 발사 직후 호출(서버·클라 모두 통해 multicast 됨).
	 * 기본 구현은 EmitterMesh 의 dynamic material 에 PulseColor 를 set 하고 시간 경과로 페이드.
	 * BP 에서 오버라이드해 Niagara·라이트·추가 사운드 등을 트리거 가능.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Tank|Membrane|Volley|Pulse")
	void OnMembraneFirePulse(FVector const& MuzzleLocationWorld);

	// ---------------------------------------------------------------------------------------

	TWeakObjectPtr<ATankEnemy> TankOwner;

	TSubclassOf<ATankMembraneProjectile> ProjectileClass;
	float ProjectileDamage = 10.f;

	int32 VolleysFiredSoFar = 0;
	FTimerHandle VolleyTimerHandle;

	/** 이번 사이클(이 양막) 동안 스폰된 살아있는 모든 투사체 — 새 볼리에서 상호 무시 등록용. */
	TArray<TWeakObjectPtr<ATankMembraneProjectile>> AliveSpawnedProjectiles;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> EmitterPulseMID;

	FTimerHandle PulseDecayTimerHandle;
	double PulseStartedWorldSeconds = 0.0;

	void FaceAggroTargetHorizontal();
	void FireOneVolley();
	void SpawnFanProjectiles();
	void NotifyTankAndDestroy();

	UMaterialInstanceDynamic* GetOrCreateEmitterPulseMID();
	void TickPulseFade();

private:
	void ScheduleNextVolley(float DelaySeconds);
};
