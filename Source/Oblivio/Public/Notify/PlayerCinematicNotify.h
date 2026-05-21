#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Cinematic/StagingCinematicTypes.h"
#include "PlayerCinematicNotify.generated.h"

/**
 * 플레이어 연출 몽타주에 배치합니다.
 * Mesh 소유자가 AOblivioCharacter 이면 HandlePlayerCinematicNotify 를 호출합니다.
 */
UCLASS(meta = (DisplayName = "Player Cinematic (CPP)"))
class OBLIVIO_API UPlayerCinematicNotify : public UAnimNotify
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Staging|Cinematic")
	EPlayerCinematicNotify NotifyEvent = EPlayerCinematicNotify::None;

	virtual FString GetNotifyName_Implementation() const override;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
