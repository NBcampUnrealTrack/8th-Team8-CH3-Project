#pragma once

// =============================================================================
// ATankEnemy — 느리고 튼튼한 변형. 근타격은 UEnemyMeleeCommitNotify(공격 Anim) 필요.
// FSM/빛/패트롤은 Base와 동일, 스탯만 탱커 쪽으로.
// =============================================================================

#include "CoreMinimal.h"
#include "AIEnemy/EnemyBase.h"
#include "TankEnemy.generated.h"

/** 탱커 기본형 — Basic 대비 체력↑·이동↓·공격력 소폭↑. 세부값은 BP에서 조정. */
UCLASS(Blueprintable)
class OBLIVIO_API ATankEnemy : public AEnemyBase
{
	GENERATED_BODY()

public:
	ATankEnemy();
};
