#include "Notify/PlayerCinematicNotify.h"

#include "OblivioCharacter.h"

FString UPlayerCinematicNotify::GetNotifyName_Implementation() const
{
	return FString::Printf(TEXT("PlayerCinematic: %s"), *UEnum::GetDisplayValueAsText(NotifyEvent).ToString());
}

void UPlayerCinematicNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp || NotifyEvent == EPlayerCinematicNotify::None)
	{
		return;
	}

	if (AOblivioCharacter* Player = Cast<AOblivioCharacter>(MeshComp->GetOwner()))
	{
		Player->HandlePlayerCinematicNotify(NotifyEvent);
	}
}
