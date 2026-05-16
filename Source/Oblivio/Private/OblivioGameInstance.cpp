#include "OblivioGameInstance.h"
#include "OblivioSaveGame.h"
#include "Kismet/GameplayStatics.h"

void UOblivioGameInstance::SaveGameData(FString SlotName)
{
	UOblivioSaveGame* SaveInstance = Cast<UOblivioSaveGame>(UGameplayStatics::CreateSaveGameObject(UOblivioSaveGame::StaticClass()));
	if (!SaveInstance)
	{
		return;
	}

	SaveInstance->SaveSlotName = SlotName;
	SaveInstance->SavedInventorySlots = SavedInventorySlots;
	SaveInstance->SavedCurrentHealth = CurrentHealth;
	SaveInstance->SavedBattery = CurrentBattery;
	SaveInstance->SavedCurrentHunger = CurrentHunger;
	SaveInstance->SavedCurrentThirst = CurrentThirst;
	SaveInstance->SavedFloor = CurrentFloor;
	SaveInstance->SavedKills = TotalKills;
	SaveInstance->SavedMementos = TotalMementos;

	if (SlotName.Len() <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("SaveGameData: Slot name is empty; aborting."));
		return;
	}

	UGameplayStatics::SaveGameToSlot(SaveInstance, SlotName, SaveInstance->UserIndex);
	UE_LOG(LogTemp, Warning, TEXT("Game Saved Successfully!"));
}

void UOblivioGameInstance::LoadGameData(FString SlotName)
{
	const uint32 UserIdx = 0;
	if (!UGameplayStatics::DoesSaveGameExist(SlotName, UserIdx))
	{
		/** 슬롯이 없는데 로드만 호출하면 GameInstance 에 이전 세션 값이 그대로 남아 시작 체력이 틀어질 수 있음 */
		ResetGameData();
		UE_LOG(LogTemp, Warning, TEXT("Save slot '%s' not found — game data reset to defaults."), *SlotName);
		return;
	}

	USaveGame* LoadedData = UGameplayStatics::LoadGameFromSlot(SlotName, UserIdx);
	UOblivioSaveGame* const LoadInstance = Cast<UOblivioSaveGame>(LoadedData);
	if (!LoadInstance)
	{
		ResetGameData();
		UE_LOG(LogTemp, Error, TEXT("LoadGameData: slot '%s' invalid type — reset to defaults."), *SlotName);
		return;
	}

	SavedInventorySlots = LoadInstance->SavedInventorySlots;
	CurrentHealth = LoadInstance->SavedCurrentHealth;
	CurrentBattery = LoadInstance->SavedBattery;
	CurrentHunger = LoadInstance->SavedCurrentHunger;
	CurrentThirst = LoadInstance->SavedCurrentThirst;
	CurrentFloor = LoadInstance->SavedFloor;
	TotalKills = LoadInstance->SavedKills;
	TotalMementos = LoadInstance->SavedMementos;

	UE_LOG(LogTemp, Warning, TEXT("Game Loaded Successfully from '%s'."), *SlotName);
}
