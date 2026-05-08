#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DoorBase.generated.h"

UCLASS()
class OBLIVIO_API ADoorBase : public AActor
{
	GENERATED_BODY()
	
public:
	ADoorBase();

	UFUNCTION(BlueprintImplementableEvent, Category = "Interaction")
	void InteractDoor();

	// 다음 레벨의 이름을 저장하는 변수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level Transition")
	FName NextLevelName;

	// 이 문이 레벨을 이동시키는 문인지 체크하는 플래그
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level Transition")
	bool bIsExitDoor = false;
};
