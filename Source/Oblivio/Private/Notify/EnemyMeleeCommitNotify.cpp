#include "Notify/EnemyMeleeCommitNotify.h"

#include "AIEnemy/EnemyBase.h"

void UEnemyMeleeCommitNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	AEnemyBase* const Enemy = Cast<AEnemyBase>(MeshComp->GetOwner());
	if (!IsValid(Enemy))
	{
		return;
	}

	Enemy->CommitAttackFromAnimNotify(nullptr);
}
