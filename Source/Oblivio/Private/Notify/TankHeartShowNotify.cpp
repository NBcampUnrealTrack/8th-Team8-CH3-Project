#include "Notify/TankHeartShowNotify.h"

#include "AIEnemy/TankEnemy.h"

void UTankHeartShowNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
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
		Tank->SetTankHeartMeshVisible(true);
	}
}
