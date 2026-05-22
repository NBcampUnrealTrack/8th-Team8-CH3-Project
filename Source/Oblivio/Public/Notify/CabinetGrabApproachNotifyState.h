#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "CabinetGrabApproachNotifyState.generated.h"

/**
 * 그랩 몽타주/시퀀스 접근 구간 — Notify State 동안 에너미가 플레이어 정면으로 미끄러져 접근.
 */
UCLASS(meta = (DisplayName = "Cabinet Grab Approach (CPP)"))
class OBLIVIO_API UCabinetGrabApproachNotifyState : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual FString GetNotifyName_Implementation() const override;

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
