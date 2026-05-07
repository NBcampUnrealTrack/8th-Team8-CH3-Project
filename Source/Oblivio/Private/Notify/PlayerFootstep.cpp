//PlayerFootstep.cpp


#include "Notify/PlayerFootstep.h"
#include "OblivioCharacter.h"

void UPlayerFootstep::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	AOblivioCharacter* PlayerCharacter = Cast<AOblivioCharacter>(MeshComp->GetOwner());
	if (IsValid(PlayerCharacter)) {
		PlayerCharacter->OnPlayerFootstep.Broadcast();
	}
}
