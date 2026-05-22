#pragma once

#include "CoreMinimal.h"
#include "AIEnemy/StagingEnemy.h"
#include "CabinetEnemy.generated.h"

class UBoxComponent;
class UAnimMontage;
class UAnimSequence;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCabinetMashProgressChanged, int32, CurrentPressCount);

/**
 * 캐비넷 안 대기 → 트리거 진입 시 문 열림 + 그랩 → E 연타 → 성공: 넉다운 / 실패: 즉사.
 * ABP·몽타주는 AStagingEnemy(연출 에너미)와 동일 에셋 사용.
 */
UCLASS(Blueprintable)
class OBLIVIO_API ACabinetEnemy : public AStagingEnemy
{
	GENERATED_BODY()

public:
	ACabinetEnemy();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual EEnemyAIState GetEnemyState() const override;

	UFUNCTION(BlueprintCallable, Category = "Cabinet|Encounter")
	void StartCabinetEncounter(AOblivioCharacter* Player);

	UFUNCTION(BlueprintPure, Category = "Cabinet|Mash")
	bool IsMashWindowActive() const { return bMashWindowActive; }

	UFUNCTION(BlueprintPure, Category = "Cabinet|Mash")
	int32 GetCurrentMashPressCount() const { return CurrentMashPressCount; }

	UFUNCTION(BlueprintPure, Category = "Cabinet|Mash")
	int32 GetRequiredMashPressCount() const { return RequiredMashPressCount; }

	UFUNCTION(BlueprintCallable, Category = "Cabinet|Mash")
	void BeginMashWindow(int32 InRequiredPressCount);

	UFUNCTION(BlueprintCallable, Category = "Cabinet|Mash")
	void EndMashWindow();

	UFUNCTION(BlueprintCallable, Category = "Cabinet|Mash")
	void RegisterMashPress();

	UFUNCTION(BlueprintCallable, Category = "Cabinet|Grab")
	void BeginGrabApproach(float Duration);

	UFUNCTION(BlueprintCallable, Category = "Cabinet|Grab")
	void UpdateGrabApproach(float DeltaSeconds);

	UFUNCTION(BlueprintCallable, Category = "Cabinet|Grab")
	void EndGrabApproach();

	UFUNCTION(BlueprintPure, Category = "Cabinet|Grab")
	bool IsGrabApproachActive() const { return bGrabApproachActive; }

	UFUNCTION(BlueprintCallable, Category = "Cabinet|Encounter")
	void OpenCabinetDoor();

	UFUNCTION(BlueprintPure, Category = "Cabinet|Encounter")
	bool IsCabinetDoorOpen() const { return bCabinetDoorOpened; }

	virtual bool ShouldPlayGrabAnimation() const override;
	virtual bool ShouldPlayKnockdownAnimation() const override;
	virtual void HandleGrabComplete() override;

	UPROPERTY(BlueprintAssignable, Category = "Cabinet|Mash")
	FCabinetMashProgressChanged OnMashProgressChanged;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cabinet|Mesh")
	TObjectPtr<UStaticMeshComponent> CabinetBodyMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cabinet|Mesh")
	TObjectPtr<UStaticMeshComponent> CabinetDoorMesh;

	/** 문 닫힘(0) → 열림 시 상대 Yaw(도). BP에서 힌지 방향에 맞게 부호 조정. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cabinet|Mesh")
	float CabinetDoorOpenYaw = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cabinet|Mesh", meta = (ClampMin = "0.01"))
	float CabinetDoorOpenDuration = 0.45f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cabinet|Trigger")
	TObjectPtr<UBoxComponent> EncounterTrigger;

	/** 플레이어 Pawn 겹침 시 1회만 연출 시작. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cabinet|Trigger")
	bool bOneShotEncounter = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cabinet|Trigger",
		meta = (DeprecatedProperty, DeprecationMessage = "Use TriggerActivationExtent for overlap size"))
	FVector TriggerExtent = FVector(120.f, 120.f, 100.f);

	/** EncounterTrigger 실제 겹침 박스 크기(플레이어 캡슐이 이 안에 들어와야 조우). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cabinet|Trigger")
	FVector TriggerActivationExtent = FVector(70.f, 70.f, 90.f);

	/** true면 트리거를 한 번 벗어났다가 다시 들어와야 조우 시작(스폰·BeginPlay 오겹침·먼 거리 오작동 방지). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cabinet|Trigger")
	bool bRequireExitBeforeEnter = true;

	/** true면 BeginPlay 시 이미 겹친 플레이어로 즉시 조우(디버그용). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cabinet|Trigger")
	bool bAllowBeginPlayOverlapStart = false;

	/** true면 플레이어 CapsuleComponent 겹침만 인정. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cabinet|Trigger")
	bool bRequirePlayerCapsuleOverlap = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cabinet|Anim")
	TObjectPtr<UAnimMontage> CabinetGrabMontage;

	/** Montage 대신 Sequence만 있을 때(에디터 스샷처럼 노티를 Sequence에 넣은 경우) 동적 몽타주로 재생. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cabinet|Anim")
	TObjectPtr<UAnimSequence> CabinetGrabAnimSequence;

	/** E 연타 성공 시 넉다운 AnimSequence (SingleNode 직접 재생). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cabinet|Anim")
	TObjectPtr<UAnimSequence> KnockdownAnimSequence;

	/** true면 넉다운을 ABP 없이 Sequence 로 직접 재생. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cabinet|Anim")
	bool bPlayKnockdownAsSingleNode = true;

	/** 그랩 시 플레이어 정면 기준으로 에너미·플레이어 위치 고정 후 Attach. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cabinet|Grab")
	bool bAttachPlayerDuringGrab = true;

	/** 플레이어 Forward 기준, 에너미를 플레이어 정면 앞에 둘 거리(cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cabinet|Grab", meta = (ClampMin = "20"))
	float GrabEnemyDistanceInFrontOfPlayer = 85.f;

	/** 에너미 → 플레이어 방향 간격(cm). 붙일수록 작게. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cabinet|Grab", meta = (ClampMin = "10"))
	float GrabPlayerHoldDistanceFromEnemy = 45.f;

	/** 비어 있으면 Mesh Root 기준 Attach. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cabinet|Grab")
	FName GrabAttachSocketName = NAME_None;

	/** 넉다운 연출 종료 후 액터 Destroy. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cabinet|Encounter")
	bool bDestroyAfterKnockdown = true;

	/** ABP 슬롯 이름 — ABP_StagingEnemy AnimGraph Slot 노드와 일치해야 함. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cabinet|Anim")
	FName GrabMontageSlotName = FName(TEXT("DefaultSlot"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cabinet|Mash", meta = (ClampMin = "1", DisplayName = "Required Mash Press Count"))
	int32 DefaultRequiredMashPressCount = 10;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Cabinet|Mash")
	bool bMashWindowActive = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Cabinet|Mash")
	int32 CurrentMashPressCount = 0;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Cabinet|Mash")
	int32 RequiredMashPressCount = 5;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Cabinet|Encounter")
	bool bEncounterTriggered = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Cabinet|Encounter")
	bool bPlayerArmedEncounterTrigger = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Cabinet|Encounter")
	bool bEnemyHiddenInCabinet = true;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Cabinet|Encounter")
	bool bCabinetDoorOpenActive = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Cabinet|Encounter")
	bool bCabinetDoorOpened = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Cabinet|Encounter")
	bool bCabinetVisualsDetachedToWorld = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Cabinet|Encounter")
	bool bCabinetVisualsPersistedInWorld = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Cabinet|Grab")
	bool bGrabPairCollisionSuppressed = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Cabinet|Grab")
	bool bLockPlayerTransformDuringGrab = false;

	FVector GrabLockedPlayerLocation = FVector::ZeroVector;
	FRotator GrabLockedPlayerRotation = FRotator::ZeroRotator;
	ECollisionResponse CachedPlayerPawnCollisionResponse = ECR_Block;
	ECollisionResponse CachedEnemyPawnCollisionResponse = ECR_Block;
	bool bCachedPlayerEnablePhysicsInteraction = true;

	float CabinetDoorOpenElapsed = 0.f;
	FRotator CabinetDoorClosedRotation = FRotator::ZeroRotator;
	FRotator CabinetDoorOpenRotation = FRotator::ZeroRotator;
	FTransform CachedCabinetBodyWorldTransform = FTransform::Identity;
	bool bCachedCabinetBodyWorldTransform = false;

	void UpdateCabinetDoorRotation(float DeltaSeconds);
	void CacheCabinetDoorClosedRotation();
	void DetachCabinetVisualsToWorld();
	void PersistCabinetVisualsInWorld();
	bool IsCabinetVisualMeshComponent(const UStaticMeshComponent* MeshComp) const;
	void GatherCabinetVisualMeshes(TArray<UStaticMeshComponent*>& OutMeshes) const;
	void ApplyGrabPairCollisionSuppression(AOblivioCharacter* Player);
	void RestoreGrabPairCollisionSuppression();
	void BeginGrabPlayerTransformLock(AOblivioCharacter* Player);
	void EnforceGrabPlayerTransformLock();
	void ReleaseGrabPlayerTransformLock();

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Cabinet|Anim")
	bool bUsingSingleNodeCinematicAnim = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Cabinet|Grab")
	bool bPlayerAttachedForGrab = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Cabinet|Grab")
	bool bGrabApproachActive = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Cabinet|Grab")
	bool bHoldGrabApproachPosition = false;

	float GrabApproachDuration = 0.f;
	float GrabApproachElapsed = 0.f;
	FVector GrabApproachStartLocation = FVector::ZeroVector;
	FRotator GrabApproachStartRotation = FRotator::ZeroRotator;
	FVector GrabApproachTargetLocation = FVector::ZeroVector;
	FRotator GrabApproachTargetRotation = FRotator::ZeroRotator;
	FVector GrabApproachReferenceForward = FVector::ZeroVector;
	FVector GrabApproachLockedLocation = FVector::ZeroVector;
	FRotator GrabApproachLockedRotation = FRotator::ZeroRotator;

	bool ComputeGrabApproachTarget(FVector& OutEnemyLocation, FRotator& OutEnemyRotation) const;
	void ClampGrabApproachLocation(FVector& InOutLocation) const;
	void EnforceGrabApproachPositionLock();

	UFUNCTION()
	void OnEncounterTriggerOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnEncounterTriggerOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	bool DoesPlayerQualifyForEncounterTrigger(const AOblivioCharacter* Player, const UPrimitiveComponent* OverlapComponent) const;

	void PlayCabinetGrabMontage();
	void PlayCinematicAnimSequence(UAnimSequence* Sequence, bool bLoop = false);
	void StopSingleNodeCinematicAnim();
	void RestoreCabinetAnimBlueprint();
	static UAnimSequence* ResolveGrabAnimSequence(UAnimMontage* Montage, UAnimSequence* FallbackSequence);
	bool TryBeginEncounterForPlayer(AOblivioCharacter* Player);
	void ResolveMashEscape(bool bSuccess);
	void ApplyMashFailureToPlayer();
	void ApplyMashEscapeSuccessKnockdown();
	void SnapAndAttachLinkedPlayerForGrab();
	void DetachLinkedPlayerFromGrab();
	void ScheduleKnockdownFinish();
	void RevealEnemyFromCabinet();

	FTimerHandle RestoreAnimBlueprintTimerHandle;

	UFUNCTION(BlueprintImplementableEvent, Category = "Cabinet|Encounter")
	void OnCabinetOpened();

	UFUNCTION(BlueprintImplementableEvent, Category = "Cabinet|Encounter")
	void OnCabinetMashResolved(bool bPlayerEscaped);

	UFUNCTION(BlueprintImplementableEvent, Category = "Cabinet|Encounter")
	void OnCabinetEncounterConsumed();
};
