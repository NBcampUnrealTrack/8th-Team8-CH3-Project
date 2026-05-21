#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Cinematic/StagingCinematicTypes.h"
#include "StagingEnemyCinematicNotify.generated.h"

/**
 * 연출용 에너미 몽타주·시퀀스에 배치합니다.
 * Mesh 소유자가 AStagingEnemy 이면 HandleStagingCinematicNotify 를 호출합니다.
 */
UCLASS(meta = (DisplayName = "Staging Enemy Cinematic (CPP)"))
class OBLIVIO_API UStagingEnemyCinematicNotify : public UAnimNotify
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Staging|Cinematic")
	EStagingEnemyCinematicNotify NotifyEvent = EStagingEnemyCinematicNotify::None;

	virtual FString GetNotifyName_Implementation() const override;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
