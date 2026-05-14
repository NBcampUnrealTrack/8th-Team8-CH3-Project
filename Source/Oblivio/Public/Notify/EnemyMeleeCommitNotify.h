#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "EnemyMeleeCommitNotify.generated.h"

/**
 * 공격 Anim Sequence 몽타주에서 재생되는 프레임에 배치합니다.
 * Mesh 소유자가 AEnemyBase 이면 CommitAttackFromAnimNotify 를 호출해 근공격 브로드캐스트를 냅니다.
 */
UCLASS(meta = (DisplayName = "Enemy Melee Hit (CPP)"))
class OBLIVIO_API UEnemyMeleeCommitNotify : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual FString GetNotifyName_Implementation() const override { return TEXT("EnemyMeleeHit"); }

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
