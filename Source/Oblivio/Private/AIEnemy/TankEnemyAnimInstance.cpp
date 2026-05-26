#include "AIEnemy/TankEnemyAnimInstance.h"

#include "AIEnemy/TankEnemy.h"
#include "GameFramework/Pawn.h"

void UTankEnemyAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	TankAnimEnemyState = EEnemyAIState::Idle;
	EnemyCruntState = EEnemyAIState::Idle;
	bIsTankHeartbeatAttackChanneling = false;
	bTankJumpAttackFsmActive = false;
	bTankMembraneFsmActive = false;
	bTankPlacentaDefenseFsmActive = false;
	bTankUsesHeartbeatAoEAttack = false;

	APawn* const OwnerPawn = TryGetPawnOwner();
	const ATankEnemy* const Tank = OwnerPawn ? Cast<ATankEnemy>(OwnerPawn) : nullptr;
	if (!IsValid(Tank))
	{
		return;
	}

	TankAnimEnemyState = Tank->GetEnemyState();
	const bool bSwingLocked = Tank->IsMeleeAttackSwingStateLocked();
	const EEnemyAIState AnimCombatState =
		bSwingLocked ? EEnemyAIState::Attack : TankAnimEnemyState;
	TankAnimEnemyState = AnimCombatState;
	EnemyCruntState = AnimCombatState;
	if (!Tank->IsAlive())
	{
		return;
	}

	bTankUsesHeartbeatAoEAttack = Tank->UsesHeartbeatAoEAttack();
	bIsTankHeartbeatAttackChanneling = Tank->IsTankHeartbeatChannelingForAnim();
	bTankJumpAttackFsmActive = Tank->IsTankJumpAttackFsmActiveForAnim();
	bTankMembraneFsmActive = Tank->IsTankMembraneFsmActiveForAnim();
	bTankPlacentaDefenseFsmActive = Tank->IsTankPlacentaDefenseActiveForAnim();
}
