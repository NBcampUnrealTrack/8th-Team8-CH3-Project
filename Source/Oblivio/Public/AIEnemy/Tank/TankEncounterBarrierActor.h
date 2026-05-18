#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "AIEnemy/EnemyBase.h"

#include "TankEncounterBarrierActor.generated.h"

class ATankEnemy;
class UBoxComponent;
class UStaticMeshComponent;

/**
 * 탱커 조우가 시작된 뒤(Begin) BlockingVolume 로 플레이어 차단(+옵션 벽 비주얼).
 * 과거에는 태반 방어 패턴 동안 우회하였으나, 기본은 첫 추격(조우)부터 동일 차단이다.
 *
 * 레벨 배치: WatchedTank 에 해당 탱커 지정. 필요 시 EncounterTrigger 오버랩으로 조우 시작.
 */
UCLASS(Blueprintable)
class OBLIVIO_API ATankEncounterBarrierActor : public AActor
{
	GENERATED_BODY()

public:
	ATankEncounterBarrierActor();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

public:
	/**
	 * OptionalWallVisual 스태틱 메시를 BlockingVolume(안쪽 박스)의 스케일된 크기에 비등방으로 맞춤.
	 * 원점 기준 중앙 정렬 메시(엔진 BasicShapes 등)일 때 안쪽 박스와 일치한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tank|EncounterBarrier",
		meta = (DisplayName = "Fit Blocking Volume To Wall Mesh (or Legacy Visual Fit)"))
	void RefreshWallVisualFitToInnerBlockingVolume();

	/** 서버 전용 — 컷신 등에서 조우 시작을 강제할 때 호출 */
	UFUNCTION(BlueprintCallable, Category = "Tank|EncounterBarrier")
	void NotifyTankEncounterStarted_Server();

	UFUNCTION(BlueprintPure, Category = "Tank|EncounterBarrier")
	bool IsBarrierBlockingPlayers() const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|EncounterBarrier")
	TObjectPtr<UBoxComponent> BlockingVolume;

	/** WatchedTank 조우 시작용(옵션). BlockingVolume 보다 약간 크게 두면 안전 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|EncounterBarrier")
	TObjectPtr<UBoxComponent> EncounterTrigger;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|EncounterBarrier")
	TObjectPtr<UStaticMeshComponent> OptionalWallVisual;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tank|EncounterBarrier")
	TObjectPtr<ATankEnemy> WatchedTank;

	/** HasValidAggroTarget() 만족 시 서버에서 조우 시작 플래그 설정 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tank|EncounterBarrier")
	bool bStartEncounterWhenTankAggro = true;

	/** EncounterTrigger 에 플레이어 폰이 겹치면 조우 시작(컷신·선행 진입 등) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tank|EncounterBarrier")
	bool bStartEncounterWhenPlayerOverlapsTrigger = false;

	/**
	 * true 이면 탱커 태반 방어 FSM 활성(ATankEnemy::IsTankPlacentaDefenseActiveForAnim) 동안만
	 * BlockingVolume 과 벽 표시 비활유지(예전 우회 처리). 기본은 false — 첫 조우부터 막음.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tank|EncounterBarrier")
	bool bBypassBarrierWhileTankPlacentaDefense = false;

	/**
	 * OptionalWallVisual 메시(스케일·회전·상대 위치 포함) AABB 에 맞춰 BlockingVolume 박스 충돌 반경(SetBoxExtent)을 맞춤.
	 * 레벨에서 보이는 벽 두께와 블록 박스가 어긋날 때 켭니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tank|EncounterBarrier")
	bool bAutoFitBlockingVolumeToWallMeshBounds = true;

	/** false 면 비주얼 Relative Scale 을 건드리지 않음(BP에서 수동 배치). bAutoFitBlockingVolumeToWallMeshBounds 가 우선된다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tank|EncounterBarrier",
		meta = (EditCondition = "!bAutoFitBlockingVolumeToWallMeshBounds"))
	bool bAutoFitWallVisualToInnerBox = false;

	/** EncounterTrigger 박스를 BlockingVolume 보다 크게 줄 때 각 축에 더할 반경 차이(플레이오버랩·기본값은 CDO 차이 반영). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tank|EncounterBarrier",
		meta = (ClampMin = "0.0"))
	FVector EncounterTriggerHalfExtentBump = FVector(10.f, 20.f, 10.f);

	/**
	 * 자동 피팅 시 OptionalWallVisual 의 Relative Rotation 을 초기화(기본값 false).
	 * true 로 두면 회전까지 리셋 → BasicShapes 같은 축 정렬 메시 자동 세팅에 유리하지만 BP에서 회전 준 상태는 무시됨.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tank|EncounterBarrier",
		meta = (EditCondition = "bAutoFitWallVisualToInnerBox"))
	bool bFitWallVisualZeroRelativeRotation = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tank|EncounterBarrier",
		meta = (ClampMin = "0.02", ClampMax = "1.0"))
	float BarrierStatePollSeconds = 0.08f;

	UPROPERTY(ReplicatedUsing = OnRep_TankEncounterBegun, VisibleInstanceOnly, BlueprintReadOnly,
		Category = "Tank|EncounterBarrier")
	bool bTankEncounterBegun = false;

	UFUNCTION()
	void OnRep_TankEncounterBegun();

private:
	FTimerHandle BarrierPollTimerHandle;

	bool bLastAppliedBlockingCollision = false;
	bool bLastAppliedVisualVisible = false;

	bool bDestroyBarrierQueued = false;

	void BindTankDelegatesIfPossible();
	void UnbindTankDelegatesIfPossible();

	UFUNCTION()
	void HandleBoundTankDied(AEnemyBase* Enemy);

	UFUNCTION()
	void HandleBoundTankTargetChanged(AEnemyBase* Enemy, AActor* NewTarget);

	UFUNCTION()
	void HandleBoundTankFsmStateChanged(AEnemyBase* Enemy, EEnemyAIState OldState, EEnemyAIState NewState);

	UFUNCTION()
	void OnEncounterTriggerOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void PollBarrierFromTank_ServerAndPresentation_AllClients();
	void TryMarkEncounterBegunFromAggro_Server();
	void RefreshPresentationFromDerivedState();

	void MarkEncounterBegun_Server();
	void DestroyBarrierIfAuthorized();

	void ApplyEncounterBarrierSizing();
	void FitBlockingVolumeToOptionalWallMeshInner();
	void FitWallVisualScaleToBlockingVolumeInner();
};
