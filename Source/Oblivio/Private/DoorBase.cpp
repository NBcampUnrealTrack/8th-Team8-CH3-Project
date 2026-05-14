#include "DoorBase.h"
#include "Kismet/GameplayStatics.h"
#include "OblivioGameInstance.h"
#include "OblivioCharacter.h"
#include "Camera/PlayerCameraManager.h" 
#include "TimerManager.h"

ADoorBase::ADoorBase()
{
	PrimaryActorTick.bCanEverTick = false;

}
void ADoorBase::InteractDoor_Implementation()
{
	if (bIsExitDoor)
	{
		AOblivioCharacter* Player = Cast<AOblivioCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));

		if (bRequiresKey && Player)
		{
			bool bHasKey = Player->InventoryComponent->HasItem(RequiredKeyID);

			if (!bHasKey)
			{
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("잠겨 있습니다. 열쇠가 필요합니다."));
				}
				return;
			}
		}

		APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
		if (PC && PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StartCameraFade(0.0f, 1.0f, FadeDuration, FLinearColor::Black, false, true);

			if (APawn* PlayerPawn = PC->GetPawn())
			{
				PlayerPawn->DisableInput(PC);
			}
		}
		GetWorldTimerManager().SetTimer(TransitionTimerHandle, this, &ADoorBase::ExecuteLevelTransition, FadeDuration, false);
	}
	else
	{
		// 일반 문 처리
	}
}

void ADoorBase::ExecuteLevelTransition()
{
	UOblivioGameInstance* GI = Cast<UOblivioGameInstance>(UGameplayStatics::GetGameInstance(GetWorld()));
	AOblivioCharacter* Player = Cast<AOblivioCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));

	if (GI && Player)
	{
		FString SlotName = "Slot1";
		GI->CurrentHealth = Player->CurrentHealth;
		GI->CurrentBattery = Player->Battery;
		GI->CurrentHunger = Player->Hunger;
		GI->CurrentThirst = Player->Thirst;
		GI->CurrentFloor += 1;

		GI->SaveGameData(SlotName); // 자동 저장

		if (!NextLevelName.IsNone())
		{
			UGameplayStatics::OpenLevel(this, NextLevelName);
		}
	}
}
