#pragma once

#include "AIEnemy/EnemyBase.h"
#include "Animation/AnimInstance.h"
#include "TankEnemyAnimInstance.generated.h"

/**
 * 비대 태아/탱커 ABP Anim Instance.
 * 상태 머신 전이: Tank Anim Enemy State(EEnemyAIState)로 비교하면 Wildcard 에러를 피할 수 있습니다.
 */
UCLASS()
class OBLIVIO_API UTankEnemyAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	/** 매 틱 갱신. 전이 규칙에서 Get Enemy State 대신 이 핀을 쓰면 Enum 타입이 확정됩니다. 탱커는 JumpAttack / Heartbeat / Membrane 포함. */
	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Tank|Anim")
	EEnemyAIState TankAnimEnemyState = EEnemyAIState::Idle;

	/**
	 * TankAnimEnemyState 와 동일. 예전 ABP 변수명(Enemy Crunt State)과 호환 —
	 * Anim Blueprint 의 Variables 에 같은 이름이 있으면 지우고 이 멤버만 사용하세요.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Tank|Anim", meta = (DisplayName = "Enemy Crunt State (synced from pawn)"))
	EEnemyAIState EnemyCruntState = EEnemyAIState::Idle;

	/** true면 심작 박동 피해 채널링 중(AI 이동 억제와 동일 구간). ABP 상태 머신 조건으로 사용 */
	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Tank|Anim")
	bool bIsTankHeartbeatAttackChanneling = false;

	/** true면 점프 착지 패턴 FSM 활성(IsTankJumpAttackFsmActiveForAnim). */
	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Tank|Anim")
	bool bTankJumpAttackFsmActive = false;

	/** true면 양막 시전 패턴(IsTankMembraneFsmActiveForAnim). */
	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Tank|Anim")
	bool bTankMembraneFsmActive = false;

	/** 태반 방어 패턴 활성(ATankEnemy::IsTankPlacentaDefenseActiveForAnim). */
	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Tank|Anim")
	bool bTankPlacentaDefenseFsmActive = false;

	/** BP에서 디버깅용: 근처 본만 심작 몽타주 쓸 때 참고 가능 */
	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Tank|Anim")
	bool bTankUsesHeartbeatAoEAttack = false;
};
