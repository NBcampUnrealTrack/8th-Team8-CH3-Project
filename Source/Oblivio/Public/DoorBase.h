#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DoorBase.generated.h"

class USoundBase;

UCLASS()
class OBLIVIO_API ADoorBase : public AActor
{
	GENERATED_BODY()
	
public:
	ADoorBase();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void InteractDoor();
	virtual void InteractDoor_Implementation();

	// 다음 레벨의 이름을 저장하는 변수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level Transition")
	FName NextLevelName;

	// 이 문이 레벨을 이동시키는 문인지 체크하는 플래그
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level Transition")
	bool bIsExitDoor = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	bool bRequiresKey = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FName RequiredKeyID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	bool bIsOpen = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level Transition")
	float FadeDuration = 1.5f;

	FTimerHandle TransitionTimerHandle;

	UFUNCTION()
	void ExecuteLevelTransition();

	UPROPERTY(EditAnywhere, Category="Audio") 
	USoundBase* DoorOpenSound;
	UPROPERTY(EditAnywhere, Category="Audio") 
	USoundBase* DoorCloseSound;
};
