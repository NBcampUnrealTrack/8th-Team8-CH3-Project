#include "Notify/TankJumpAttackAnimNotifies.h"

#include "AIEnemy/TankEnemy.h"

static void DispatchTankJumpNotify(ATankEnemy* Tank, void (ATankEnemy::*Fn)())
{
	if (!IsValid(Tank) || !Fn)
	{
		return;
	}
	(Tank->*Fn)();
}

void UTankJumpLiftOffNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	if (!MeshComp)
	{
		return;
	}
	DispatchTankJumpNotify(Cast<ATankEnemy>(MeshComp->GetOwner()), &ATankEnemy::JumpAttack_NotifyLiftOff);
}

void UTankJumpLandingNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	if (!MeshComp)
	{
		return;
	}
	DispatchTankJumpNotify(Cast<ATankEnemy>(MeshComp->GetOwner()), &ATankEnemy::JumpAttack_NotifyLandingImpact);
}

void UTankJumpFinishedNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	if (!MeshComp)
	{
		return;
	}
	DispatchTankJumpNotify(Cast<ATankEnemy>(MeshComp->GetOwner()), &ATankEnemy::JumpAttack_NotifyMontageFinished);
}
