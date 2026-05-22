#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "CabinetMashWindowNotifyState.generated.h"

/**
 * 캐비넷 그랩 몽타주 구간 — Notify State 동안 E 연타로 탈출.
 * RequiredPressCount 를 노티 인스턴스마다 조정해 밸런스.
 */
UCLASS(meta = (DisplayName = "Cabinet Mash Window (CPP)"))
class OBLIVIO_API UCabinetMashWindowNotifyState : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cabinet|Mash", meta = (ClampMin = "1"))
	int32 RequiredPressCount = 10;

	virtual FString GetNotifyName_Implementation() const override;

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
