#include "Notify/TankHeartbeatDamageNotify.h"

#include "AIEnemy/TankEnemy.h"

void UTankHeartbeatDamageNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
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
		Tank->PulseTankHeartDamageFlash();
		Tank->ApplyHeartbeatDamageFromAnimNotify();
	}
}
