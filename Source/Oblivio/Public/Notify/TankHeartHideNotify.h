#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "TankHeartHideNotify.generated.h"

/** 심장 메시 숨김 — 탱커 공격 애님 타임라인에 배치합니다. */
UCLASS(meta = (DisplayName = "Tank Heart Hide (CPP)"))
class OBLIVIO_API UTankHeartHideNotify : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual FString GetNotifyName_Implementation() const override { return TEXT("TankHeartHide"); }

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
