#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AIEnemy/Tank/TankMembraneAnimTypes.h"
#include "TankMembraneEmitterActor.generated.h"

class ATankEnemy;
class ATankMembraneProjectile;
class UMaterialInstanceDynamic;
class USoundBase;
class UMaterialInterface;
class UStaticMeshComponent;
class UArrowComponent;

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

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_ShowProjectileTelegraph(FVector TelegraphOriginWorld, FVector TelegraphBaseForwardNorm,
		float TelegraphFanHalfDeg, float TelegraphSpawnFwdCm, float TelegraphSpawnUpCm,
		float TelegraphRangeUU, float TelegraphDurationSec);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Membrane")
	TObjectPtr<UStaticMeshComponent> EmitterMesh;

	/**
	 * 포신·총구 방향. 월드에서 이 컴포넌트의 Forward 가 부채꼴 중앙 발사 축(+ bInvertMuzzleDirectionArrowForward).
	 * 양막 BP 에서 EmitterMesh 기준 위치·회전만 조정하면 됨(게임 중 숨김).
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Membrane",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UArrowComponent> MuzzleDirectionArrow;

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
	// Aim — 타겟·포신 회전 미세 조정 (BP 디테일)
	// ---------------------------------------------------------------------------------------

	/** false면 볼리마다 플레이어/타겟 쪽으로 액터 Yaw 회전 안 함(MuzzleDirectionArrow 기준 각만 사용). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Membrane|Volley|Aim",
		meta = (ToolTip = "끄면 스폰 시점 회전+MuzzleArrow 각만 적용합니다."))
	bool bFaceAggroTargetEachVolley = true;

	/**
	 * true면 기존대로 수평(XY) 평면으로만 회전하여 플레이어 높이(위·아래)는 무시.
	 * false면 액터 위치에서 타겟까지 3축 방향 전체로 바라본다(FindLookAtRotation).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Membrane|Volley|Aim",
		meta = (EditCondition = "bFaceAggroTargetEachVolley",
			ToolTip = "꺼서 플레이어 허벅지 높이·점프 높이를 맞춥니다."))
	bool bFlattenAimToHorizontalPlaneWhenFacingAggro = true;

	/**
	 * MuzzleDirectionArrow 로 구한 발사축에 가산되는 회전(deg). 블프에서 총구가 플레이어를 놓치면 조정.
	 * Yaw 우측 +, Pitch 위쪽 + 는 FRotator 관례(UE 디테일과 동일).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Membrane|Volley|Aim")
	FRotator ProjectileLaunchRotatorOffsetDegrees = FRotator::ZeroRotator;

	/**
	 * 메쉬 import 축 때문에 ‘입’ 방향이 Arrow Forward 와 180° 반대일 때 체크.
	 * true 이면 Arrow.GetForwardVector 대신 역방향으로 발사축을 잡는다(스폰 오프셋·부채꼴도 같은 축).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Membrane|Volley|Aim",
		meta = (ToolTip =
			"Arrow 컴포넌트 로컬 Forward(+X)가 실제 총구 밖쪽과 반대이면 체크. 발사·전방 오프셋 축을 Arrow 역방향으로 바꿉니다."))
	bool bInvertMuzzleDirectionArrowForward = false;

	// ---------------------------------------------------------------------------------------
	// Telegraph — 부채꼴 3발 각각 예상 직선(실린더 튜브). 같은 볼리에서 투사체 스폰 직후 NetMulticast 표시.
	// 발사 각·오프셋은 SpawnFanProjectiles 와 동일 벡터. 반투명 머티리얼은 BP 에서 ProjectileTelegraphMaterial 로 지정.
	// 업그레이드: 같은 파라미터로 Niagara Beam 교체 또는 Decal 레이어만 추가하면 됨.
	// ---------------------------------------------------------------------------------------

	/** 각 볼리마다 3방향 예측 라인 표시(NetMulticast 클라 포함). 끄면 양방 비활성. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Membrane|Telegraph")
	bool bShowProjectilePathTelegraph = true;

	/** 스폰 직선 시작(총구 오프셋 적용 후)부터 그릴 거리(cm). 실제 라이프/벽충돌과 무관 · 가독용. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Membrane|Telegraph", meta = (ClampMin = "400.0"))
	float ProjectileTelegraphRangeUU = 10000.f;

	/** 라인 노출 시간(초). 0 에 가깝게 하면 깜박임이 짧음. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Membrane|Telegraph", meta = (ClampMin = "0.05"))
	float ProjectileTelegraphVisibleSeconds = 0.5f;

	/** BasicShapes Cylinder 기본 반경(50cm) 대비 타원 반경(cm). 거칠게 굵기 튜닝. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Membrane|Telegraph", meta = (ClampMin = "1.0"))
	float ProjectileTelegraphTubeRadiusCm = 22.f;

	/** 비어 있으면 Engine BasicShapeMaterial. 반투명 컬러 에미시브 등은 여기 넣거나 BP 디테일 MI. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Membrane|Telegraph")
	TObjectPtr<UMaterialInterface> ProjectileTelegraphMaterial;

	// ---------------------------------------------------------------------------------------
	// Muzzle 오프셋 — 투사체 스폰 위치를 양막 액터 원점에서 얼마나 띄울지.
	// 양막 본체가 바닥/벽에 박혀 있어 투사체가 즉시 충돌→정지하는 문제를 막는다.
	// Forward(앞)는 MuzzleDirectionArrow 의 직진 — 없으면 액터 전방. Up(위)는 월드 +Z 방향.
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

	/** Telegraph 실린더 3개(부채꼴별 라인 X). ctor 에서 채운다. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Tank|Membrane|Telegraph")
	TArray<TObjectPtr<UStaticMeshComponent>> ProjectileTelegraphTubes;

	FTimerHandle ProjectileTelegraphHideTimerHandle;

	/** 마지막 볼리 후 텔레그래프가 보이도록 Destroy 를 잠시 미룸. */
	FTimerHandle DeferredDestroyAfterTelegraphTimerHandle;

	void FaceAggroTowardTargetActor();
	static void AppendLaunchRotOffsetToNormalizedForward(FVector& InOutForward, FRotator const OffsetDegrees);
	void FireOneVolley();
	void SpawnFanProjectiles();
	void StopVolleyTimerAndNotifyTankFinished();
	void DestroyMembraneEmitterNow();
	UFUNCTION()
	void OnDeferredDestroyAfterTelegraph_TimerFired();

	UMaterialInstanceDynamic* GetOrCreateEmitterPulseMID();
	void TickPulseFade();
	void HideProjectileTelegraphVisuals();

private:
	void ScheduleNextVolley(float DelaySeconds);
};
