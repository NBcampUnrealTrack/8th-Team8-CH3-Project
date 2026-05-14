#pragma once

// =============================================================================
// AStalkerEnemy — 절름발이 추격자(스토커) 템플릿.
// 비추격(Idle·Patrol·Investigate 등)은 느린 MoveSpeed, Chase는 ChaseMoveSpeed.
// 플레이어 등 뒤에서 Chase 진입 시 단기 ChaseBurstSpeedMultiplier 적용.
// 손전등 켜짐 + 배터리 + 스포트 콘·거리 안: 메시 표시·소리. 밖이면 투명·무음.
// 메시·애니·루프 사운드는 BP Class Defaults에서 지정.
// 근공격 타격은 공격 Anim 의 UEnemyMeleeCommitNotify 필요.
// =============================================================================

#include "CoreMinimal.h"
#include "AIEnemy/EnemyBase.h"
#include "Sound/SoundBase.h"
#include "StalkerEnemy.generated.h"

class UAudioComponent;

UCLASS(Blueprintable)
class OBLIVIO_API AStalkerEnemy : public AEnemyBase
{
	GENERATED_BODY()

public:
	AStalkerEnemy();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "Enemy|Stalker")
	bool IsRevealedByFlashlight() const { return bRevealedByFlashlight; }

	/** Shipping 제외: 머리 위에 체력 문자열(DrawDebugString). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Stalker|Debug")
	bool bDebugDrawStalkerHealth = true;

protected:
	virtual void NotifyEnemyStateChanged(EEnemyAIState OldState, EEnemyAIState NewState) override;
	virtual float GetLocomotionBaseSpeed() const override;
	virtual void Die() override;
	virtual void ApplyEnemySoundVolumes() override;

	/** 손전등에 비칠 때만 보이기/들리기. 끄면 항상 표시(다른 적과 동일). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Stalker|Flashlight")
	bool bFlashlightStealth = true;

	/** 손전등 콘 검사 기준 위치(보통 메시 중심). Z 오프셋만 조정해도 됨. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Stalker|Flashlight")
	FVector FlashlightTestPointOffset = FVector(0.f, 0.f, 40.f);

	/** 콘 안으로 판정할 때 반경(cm)을 이 만큼 넉넉히. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Stalker|Flashlight", meta = (ClampMin = "0.0"))
	float FlashlightConeRadiusSlack = 32.f;

	/** 손전등에 들어왔을 때 재생할 루프(발소리·신음 등). 비우면 오디오 컴포넌트만 음소거 토글. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Stalker|Audio")
	TObjectPtr<USoundBase> StalkerRevealLoopSound;

	/** 손전등이 들어올 때 루프 볼륨 0→1까지 걸리는 시간(초). 짧을수록 빨리 선명해짐. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Stalker|Audio", meta = (ClampMin = "0.01", ClampMax = "2.0"))
	float StalkerAudioAttackSeconds = 0.08f;

	/** 손전등을 뗄 때 1→0 페이드(초). 길수록 끊김 덜함. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Stalker|Audio", meta = (ClampMin = "0.05", ClampMax = "3.0"))
	float StalkerAudioReleaseSeconds = 0.38f;

	UFUNCTION(BlueprintNativeEvent, Category = "Enemy|Stalker")
	void OnFlashlightRevealChanged(bool bRevealed);
	virtual void OnFlashlightRevealChanged_Implementation(bool bRevealed);

	/** Chase 진입 시 등 뒤 기습 버스트 지속 시간(초). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Stalker", meta = (ClampMin = "0.0"))
	float ChaseBurstDuration = 2.5f;

	/** 버스트 중 Chase·Attack 기준 이속에 곱함. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Stalker", meta = (ClampMin = "1.0"))
	float ChaseBurstSpeedMultiplier = 1.35f;

	/** true면 플레이어 시야 뒤(반구)에서만 버스트. false면 Chase 진입마다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Stalker")
	bool bChaseBurstOnlyFromBehind = true;

	/**
	 * 플레이어 Forward와 (적→플레이어) 방향의 내적이 이 값보다 작으면 "등 뒤".
	 * 0 = 정확히 측면부터 뒤, 음수일수록 더 넓은 뒤 편.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Stalker", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float BehindPlayerDotThreshold = 0.05f;

private:
	float ChaseBurstTimeRemaining = 0.0f;
	bool bRevealedByFlashlight = false;
	/** 손전등 루프 SFX용 0~1 스무딩(메시·IsRevealed는 즉시, 소리만 지연). */
	float StealthAudioEnvelope = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Stalker|Audio", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAudioComponent> StalkerAudioComponent;

	void UpdateFlashlightReveal();
	void ApplyFlashlightReveal(bool bRevealed);
	bool ComputeInPlayerFlashlightCone() const;
	bool ShouldTriggerChaseBurstFromBehind() const;

	float GetStealthAudioEnvelopeTarget() const;
	void UpdateStalkerStealthAudioEnvelope(float DeltaSeconds);
};
