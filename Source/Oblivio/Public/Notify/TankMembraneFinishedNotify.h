#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "TankMembraneFinishedNotify.generated.h"

/** 양막 연출 종료 시점에 호출되어 Membrane 상태 정리 게이트에 반영된다. 소환 노티와 별개. */
UCLASS(meta = (DisplayName = "Tank Membrane Finished (CPP)"))
class OBLIVIO_API UTankMembraneFinishedNotify : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual FString GetNotifyName_Implementation() const override { return TEXT("TankMembraneFinished"); }

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
