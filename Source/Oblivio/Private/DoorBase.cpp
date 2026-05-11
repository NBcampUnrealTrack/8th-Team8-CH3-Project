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
		GI->CurrentHealth = Player->CurrentHealth;
		GI->CurrentBattery = Player->Battery;
		GI->CurrentHunger = Player->Hunger;
		GI->CurrentThirst = Player->Thirst;
		GI->CurrentFloor += 1;

		GI->SaveGameData(); // 자동 저장

		if (!NextLevelName.IsNone())
		{
			UGameplayStatics::OpenLevel(this, NextLevelName);
		}
	}
}
