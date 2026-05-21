#pragma once

// =============================================================================
// AStagingEnemy — 시작 방 등 연출 전용 적. 플레이어에게 데미지를 주지 않습니다.
// AnimNotify(EStagingEnemyCinematicNotify) + 플레이어 몽타주(EPlayerCinematicNotify)로
// 붙잡기 → 대치 → 밀치기 → 넘어짐 → 손전등 ON → 빛 데미지 → 사망 연출을 구성합니다.
// =============================================================================

#include "CoreMinimal.h"
#include "AIEnemy/EnemyBase.h"
#include "Cinematic/StagingCinematicTypes.h"
#include "StagingEnemy.generated.h"

class AOblivioCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FStagingEnemyCinematicStateChanged,
	AStagingEnemy*, Enemy, EStagingEnemyCinematicState, NewState);

UCLASS(Blueprintable)
class OBLIVIO_API AStagingEnemy : public AEnemyBase
{
	GENERATED_BODY()

public:
	AStagingEnemy();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	virtual EEnemyAIState GetEnemyState() const override;

	virtual void CommitAttackFromAnimNotify(AActor* OptionalTargetOverride = nullptr) override;
	virtual void DispatchEnemyAttackCommitted(AActor* Target, float DamageAmount,
		TSubclassOf<UDamageType> DamageTypeClass) override;

	virtual void Die() override;

	UFUNCTION(BlueprintCallable, Category = "Staging|Cinematic")
	void StartOpeningCinematic(AOblivioCharacter* Player);

	UFUNCTION(BlueprintCallable, Category = "Staging|Cinematic")
	void SetStagingState(EStagingEnemyCinematicState NewState);

	UFUNCTION(BlueprintPure, Category = "Staging|Cinematic")
	EStagingEnemyCinematicState GetStagingState() const { return StagingState; }

	UFUNCTION(BlueprintCallable, Category = "Staging|Cinematic")
	void HandleStagingCinematicNotify(EStagingEnemyCinematicNotify NotifyEvent);

	/** 대치 몽타주 프레임에서 자동 밀치기(연출). AnimNotify 또는 BP에서 호출. */
	UFUNCTION(BlueprintCallable, Category = "Staging|Cinematic")
	void ExecuteAutoPush();

	UFUNCTION(BlueprintCallable, Category = "Staging|Cinematic")
	void ForceFlashlightOnPlayer();

	UFUNCTION(BlueprintCallable, Category = "Staging|Cinematic")
	void ApplyCinematicLightDamage();

	UFUNCTION(BlueprintCallable, Category = "Staging|Cinematic")
	void FinishCinematicDeath();

	UFUNCTION(BlueprintPure, Category = "Staging|Cinematic")
	AOblivioCharacter* GetLinkedPlayer() const { return LinkedPlayer.Get(); }

	UPROPERTY(BlueprintAssignable, Category = "Staging|Cinematic")
	FStagingEnemyCinematicStateChanged OnStagingCinematicStateChanged;

	/** BP에서 넘어짐·사망 몽타주 재생 등 추가 연출용. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Staging|Cinematic")
	void OnStagingCinematicNotify(EStagingEnemyCinematicNotify NotifyEvent);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Staging|Cinematic")
	EStagingEnemyCinematicState StagingState = EStagingEnemyCinematicState::Idle;

	UPROPERTY(Transient)
	TWeakObjectPtr<AOblivioCharacter> LinkedPlayer;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Staging|Cinematic")
	bool bAutoStartOpeningCinematic = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Staging|Cinematic", meta = (ClampMin = "0.0"))
	float AutoStartDelaySeconds = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Staging|Cinematic", meta = (ClampMin = "0.0"))
	float PushKnockbackStrength = 900.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Staging|Cinematic", meta = (ClampMin = "0.0"))
	float PushKnockbackUpward = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Staging|Cinematic", meta = (ClampMin = "0.0"))
	float CinematicLightDamage = 9999.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Staging|Cinematic")
	bool bLockPlayerLookAtEnemyDuringGrab = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Staging|Cinematic", meta = (ClampMin = "0.0"))
	float GrabLookLockDuration = 8.f;

	void EnterCinematicMode();
	void HandleGrabComplete();
	void HandleEnterStandoff();
	void HandleExecuteAutoPush();
	void HandleKnockdownBegin();

	UPROPERTY(Transient)
	bool bCinematicModeActive = false;
};
