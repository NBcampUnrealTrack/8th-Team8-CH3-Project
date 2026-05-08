#pragma once

#include "CoreMinimal.h"
#include "GameFramework/DamageType.h"
#include "LightDamageType.generated.h"

/** Light attack damage marker. CC is handled by AEnemyBase::OnLightHit, not generic damage stun. */
UCLASS()
class OBLIVIO_API ULightDamageType : public UDamageType
{
	GENERATED_BODY()
};
