#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlayerCombatComponent.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class OBLIVIO_API UPlayerCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlayerCombatComponent();

protected:
	virtual void BeginPlay() override;
};
