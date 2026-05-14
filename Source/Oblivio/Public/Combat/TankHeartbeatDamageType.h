#pragma once

#include "CoreMinimal.h"
#include "GameFramework/DamageType.h"
#include "TankHeartbeatDamageType.generated.h"

/**
 * 탱커 심장박동(Heartbeat) 공격 전용 마커 타입.
 * 일반 근접(UDamageType)·손전등(ULightDamageType)과 구분해 UI/리액션/저항에 사용할 수 있습니다.
 */
UCLASS()
class OBLIVIO_API UTankHeartbeatDamageType : public UDamageType
{
	GENERATED_BODY()
};
