#include "Notify/TankHeartHideNotify.h"

#include "AIEnemy/TankEnemy.h"

void UTankHeartHideNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	ATankEnemy* const Tank = Cast<ATankEnemy>(MeshComp->GetOwner());
	if (IsValid(Tank))
	{
		Tank->SetTankHeartMeshVisible(false);
	}
}
