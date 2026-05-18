#include "DoorBase.h"
#include "Kismet/GameplayStatics.h"
#include "OblivioGameInstance.h"
#include "OblivioCharacter.h"
#include "Camera/PlayerCameraManager.h" 
#include "TimerManager.h"
#include "Sound/SoundBase.h"
#include "Items/OblivioInventoryComponent.h"
#include "OblivioGameMode.h"

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
			Player->InventoryComponent->ConsumeItem(EItemType::Key, 1);

			if (AOblivioGameMode* GM = Cast<AOblivioGameMode>(UGameplayStatics::GetGameMode(GetWorld())))
			{
				GM->CollectedKeys--;
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
		bIsOpen = !bIsOpen;

		USoundBase* TargetSound = bIsOpen ? DoorOpenSound : DoorCloseSound;

		// 에셋이 비어있지 않은지 검사 후, 문이 있는 3D 위치에서 재생
		if (IsValid(TargetSound))
		{
			UGameplayStatics::PlaySoundAtLocation(this, TargetSound, GetActorLocation());
		}
	}
}

void ADoorBase::ExecuteLevelTransition()
{
	UOblivioGameInstance* GI = Cast<UOblivioGameInstance>(UGameplayStatics::GetGameInstance(GetWorld()));
	AOblivioCharacter* Player = Cast<AOblivioCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));

	if (GI && Player)
	{
		FString SlotName = "Slot1";
		Player->CurrentHealth = GI->CurrentHealth;
		Player->Battery = GI->CurrentBattery;
		Player->Hunger = GI->CurrentHunger;
		Player->Thirst = GI->CurrentThirst;
		GI->CurrentFloor -= 1;

		GI->SaveGameData(SlotName); // 자동 저장

		if (!NextLevelName.IsNone())
		{
			UGameplayStatics::OpenLevel(this, NextLevelName);
		}
	}
}
