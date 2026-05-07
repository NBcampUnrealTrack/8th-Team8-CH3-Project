//PlayerThrow.cpp


#include "Notify/PlayerThrow.h"
#include "OblivioCharacter.h"

void UPlayerThrow::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	AOblivioCharacter* PlayerCharacter = Cast<AOblivioCharacter>(MeshComp->GetOwner());
	if (IsValid(PlayerCharacter)) {
		PlayerCharacter->OnPlayerThrow.Broadcast();
	}
}