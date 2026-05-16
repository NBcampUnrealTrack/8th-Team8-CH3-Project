#include "Notify/TankMembraneSummonNotify.h"

#include "AIEnemy/TankEnemy.h"

void UTankMembraneSummonNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	if (ATankEnemy* const Tank = Cast<ATankEnemy>(MeshComp->GetOwner()))
	{
		Tank->TankMembrane_NotifySummon();
	}
}
