#include "Notify/StagingEnemyCinematicNotify.h"

#include "AIEnemy/StagingEnemy.h"

FString UStagingEnemyCinematicNotify::GetNotifyName_Implementation() const
{
	return FString::Printf(TEXT("StagingEnemy: %s"), *UEnum::GetDisplayValueAsText(NotifyEvent).ToString());
}

void UStagingEnemyCinematicNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp || NotifyEvent == EStagingEnemyCinematicNotify::None)
	{
		return;
	}

	if (AStagingEnemy* StagingEnemy = Cast<AStagingEnemy>(MeshComp->GetOwner()))
	{
		StagingEnemy->HandleStagingCinematicNotify(NotifyEvent);
	}
}
