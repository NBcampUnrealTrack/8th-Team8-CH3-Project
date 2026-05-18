#pragma once

#include "CoreMinimal.h"
#include "AIEnemy/EnemyBase.h"
#include "WhisperEnemy.generated.h"

class AAIController;
class UAudioComponent;
class USoundBase;

/**
 * AWhisperEnemy - "속삭이는 자"
 * - 원거리 DoT 추적: WhisperFightMinDistance ~ WhisperRange 도넛에서 초당 WhisperDotDamagePerSecond (기본적으로 시야 채널로 벽 차단 시 미적용).
 * - Chase·Attack 동안 WhisperFightMinDistance 안으로 붙지 않음(외곽 호흡 거리 조절).
 * - 손전등 콘 안에서는 회피 이동(AvoidFlashlightCone). 빛 CC는 슬로우만, 경직은 무시.
 */
UCLASS(Blueprintable)
class OBLIVIO_API AWhisperEnemy : public AEnemyBase
{
	GENERATED_BODY()

public:
	AWhisperEnemy();

	virtual void ApplyCCStun(float Duration = 0.0f) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void UpdateChase() override;
	virtual void UpdateAttack() override;
	virtual bool IsObstacleAttackEnabled() const override { return false; }
	virtual void PerformAttack_Implementation(AActor* Target) override;
	virtual float GetLocomotionBaseSpeed() const override;
	virtual bool IsTargetInAttackRange() const override;
	virtual bool IsMeleeCommitNotifyHitValid(AActor const* HitTarget) const override;
	virtual float GetMeleeAttackRangeIndicatorRadiusCm() const override { return WhisperRange; }
	virtual void ApplyEnemySoundVolumes() override;

	/** 원거리 DoT 초당 피해(직접 타격 없음). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Whisper|Damage", meta = (ClampMin = "0.0"))
	float WhisperDotDamagePerSecond = 3.0f;

	/** 이 거리 안으로 접근하지 않음(cm, 수평). DoT는 이 바깥~WhisperRange 안에서만. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Whisper", meta = (ClampMin = "0.0"))
	float WhisperFightMinDistance = 200.0f;

	/** Whisper DoT 적용 거리(cm, 수평). 최대 간격 한계. WhisperFightMinDistance 보다 반드시 큼. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Whisper", meta = (ClampMin = "50.0"))
	float WhisperRange = 550.0f;

	/** true면 속삭임 DoT·원거리 판정에 적→플레이어 ECC_Visibility 라인 검사. 차단 물체가 목표 근처가 아니면 피해·루프 없음. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Whisper|Sight")
	bool bRequireLineOfSightForWhisperDot = true;

	/** 막히는 표면이 목표 캡슐까지 이 거리(cm) 안에 있으면 시야 성공으로 본다(부동 소수·모서리). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Whisper|Sight", meta = (ClampMin = "0.0"))
	float WhisperDotLosClearanceCm = 35.f;

	/** 손전등 위험 콘 회피 시 이동 기준 속도(cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Whisper|Avoid", meta = (ClampMin = "0.0"))
	float WhisperFlashlightAvoidMoveSpeed = 300.0f;

	/** 손전등 콘 거리에 더하는 안전 여유(cm). 시각 빛과 거의 일치시키려면 0~50 권장. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Whisper|Avoid", meta = (ClampMin = "0.0"))
	float DangerConeRadiusSlack = 30.0f;

	/** 손전등 콘 반각에 더하는 안전 마진(deg). 시각 빛과 거의 일치시키려면 0~5 권장. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Whisper|Avoid", meta = (ClampMin = "0.0", ClampMax = "60.0"))
	float DangerConeAngleMarginDeg = 5.0f;

	/** 화면/월드에 콘과 InDanger 결과를 그려서 회피 판정을 시각화. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Whisper|Debug")
	bool bDebugDrawFlashlightDanger = false;

	/**
	 * 속삭임 DoT가 유효한 동안 WhisperAttackAudioComponent 로 재생(루프 Cue 권장).
	 * 종료 시 FadeOut 처리됩니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Whisper|Audio")
	TObjectPtr<USoundBase> WhisperAttackSound;

	/** 속삭임 루프가 켜질 때 FadeIn 시간(초). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Whisper|Audio", meta = (ClampMin = "0.01"))
	float WhisperAttackSoundFadeInDuration = 0.25f;

	/** 속삭임 세그먼트가 끝날 때 FadeOut 시간(초). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Whisper|Audio", meta = (ClampMin = "0.05"))
	float WhisperAttackSoundFadeOutDuration = 0.35f;

	/** 속삭임 루프용(루트에 붙임). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Whisper|Audio")
	TObjectPtr<UAudioComponent> WhisperAttackAudioComponent;

	/**
	 * EnemySoundVolumeMultiplier와 곱해져 속삭임 볼륨만 따로 줄입니다.
	 * (예: 속삭임만 작게 들리게 할 때 유용합니다.)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Whisper|Audio", meta = (ClampMin = "0.0"))
	float WhisperAttackSoundVolumeScale = 1.0f;

	/** 속삭임 DoT가 적용되는 틱마다 호출(추가 레이어만 BP/C++ 에서 처리). 속삭임 루프는 WhisperAttackAudioComponent 가 담당합니다. */
	UFUNCTION(BlueprintNativeEvent, Category = "Enemy|Whisper|Audio")
	void NotifyWhisperAttackSound();

private:
	bool IsWithinWhisperRange() const;
	/** 목표에게 시야 채널 차단 없이 속삭임을 줄 수 있는가(bRequire 미사용이면 항상 true). */
	bool HasWhisperDotLineOfSightToTarget() const;
	bool PassesWhisperCombatEngagementBaseline() const;

	bool IsPointInsideFlashlightDanger(const FVector& Point) const;
	bool IsSelfInsideFlashlightDanger() const;
	void MaintainEngagementDistance(AAIController* AI);
	void AvoidFlashlightCone(AAIController* AI);
	void TickWhisperDotDamage(float DeltaSeconds);

	void TickWhisperAttackLoopAudio();

	/** 속삭임 사운드를 실제로 켜거나 끄는 상태(페이드 트랜지션 1회용). */
	bool bWhisperAttackAudioTrackedOn = false;
};
