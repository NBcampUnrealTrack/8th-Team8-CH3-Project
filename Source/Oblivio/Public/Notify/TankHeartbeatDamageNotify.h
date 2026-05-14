#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "TankHeartbeatDamageNotify.generated.h"

/**
 * 심장 박동 공격 애님에서 박동 피해 프레임마다 배치합니다(노티 개수 = 타격 횟수).
 * PulseTankHeartDamageFlash(붉은 점등) + ApplyHeartbeatDamageFromAnimNotify → UTankHeartbeatDamageType.
 */
UCLASS(meta = (DisplayName = "Tank Heartbeat Damage (CPP)"))
class OBLIVIO_API UTankHeartbeatDamageNotify : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual FString GetNotifyName_Implementation() const override { return TEXT("TankHeartbeatDamage"); }

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
