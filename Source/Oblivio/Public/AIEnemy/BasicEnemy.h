#pragma once

// =============================================================================
// ABasicEnemy — 범용 근접형 적. 로직은 전부 AEnemyBase, 여기선 C++ 기본 스탯만 설정.
// 근공격 타격은 공격 Anim 의 UEnemyMeleeCommitNotify 로 CommitAttackFromAnimNotify 호출.
// 메시·애니·콜리전·세부 밸런스는 블루프린트 Class Defaults에서 조정.
// =============================================================================

#include "CoreMinimal.h"
#include "AIEnemy/EnemyBase.h"
#include "BasicEnemy.generated.h"

UCLASS(Blueprintable)
class OBLIVIO_API ABasicEnemy : public AEnemyBase
{
	GENERATED_BODY()

public:
	ABasicEnemy();
};
