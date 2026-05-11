#pragma once

#include "CoreMinimal.h"
#include "AIEnemy/EnemyBase.h"
#include "WhisperEnemy.generated.h"

class AAIController;

/**
 * AWhisperEnemy - "속삭이는 자"
 * - 배회/도주 없이 플레이어에게 계속 접근
 * - 근접하면 공격 판단만 PerformAttack으로 위임
 * - 손전등 콘 안에서는 회피 이동(AvoidFlashlightCone). 빛(저데미지) CC는 슬로우만 적용되고 경직(Stun)은 무시.
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
	virtual void UpdateChase() override;
	virtual void UpdateAttack() override;
	virtual bool IsObstacleAttackEnabled() const override { return false; }

	/** 손전등 OFF 및 공격 판단을 수행하는 수평 거리(cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Whisper", meta = (ClampMin = "50.0"))
	float WhisperRange = 150.0f;

	/** 손전등 콘 거리에 더하는 안전 여유(cm). 시각 빛과 거의 일치시키려면 0~50 권장. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Whisper|Avoid", meta = (ClampMin = "0.0"))
	float DangerConeRadiusSlack = 30.0f;

	/** 손전등 콘 반각에 더하는 안전 마진(deg). 시각 빛과 거의 일치시키려면 0~5 권장. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Whisper|Avoid", meta = (ClampMin = "0.0", ClampMax = "60.0"))
	float DangerConeAngleMarginDeg = 5.0f;

	/** 화면/월드에 콘과 InDanger 결과를 그려서 회피 판정을 시각화. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Whisper|Debug")
	bool bDebugDrawFlashlightDanger = false;

private:
	float NextAttackDecisionTime = 0.0f;

	bool IsWithinWhisperRange() const;
	bool IsPointInsideFlashlightDanger(const FVector& Point) const;
	bool IsSelfInsideFlashlightDanger() const;
	void ApproachTarget(AAIController* AI);
	void AvoidFlashlightCone(AAIController* AI);
	void TryCommitWhisperAttack();
};
