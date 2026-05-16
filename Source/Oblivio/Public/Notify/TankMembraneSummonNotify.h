#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "TankMembraneSummonNotify.generated.h"

/** 양막 애님 시퀀스에서 이 노티가 찍힌 프레임에 실제 양막 이펙터/투사체를 소환합니다. */
UCLASS(meta = (DisplayName = "Tank Membrane Summon (CPP)"))
class OBLIVIO_API UTankMembraneSummonNotify : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual FString GetNotifyName_Implementation() const override { return TEXT("TankMembraneSummon"); }

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
