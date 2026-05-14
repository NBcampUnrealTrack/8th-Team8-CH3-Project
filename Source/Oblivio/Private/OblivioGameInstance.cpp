#include "OblivioGameInstance.h"
#include "OblivioSaveGame.h"
#include "Kismet/GameplayStatics.h"

void UOblivioGameInstance::SaveGameData(FString SlotName)
{
	//세이브 객체 생성
	UOblivioSaveGame* SaveInstance = Cast<UOblivioSaveGame>(UGameplayStatics::CreateSaveGameObject(UOblivioSaveGame::StaticClass()));

	//현재 GameInstance의 데이터를 세이브 객체로 복사
	SaveInstance->SavedInventorySlots = SavedInventorySlots;
	UGameplayStatics::SaveGameToSlot(SaveInstance, SaveInstance->SaveSlotName, SaveInstance->UserIndex);
	SaveInstance->SavedCurrentHealth = CurrentHealth;
	SaveInstance->SavedBattery = CurrentBattery;
	SaveInstance->SavedCurrentHunger = CurrentHunger;
	SaveInstance->SavedCurrentThirst = CurrentThirst;
	SaveInstance->SavedFloor = CurrentFloor;
	SaveInstance->SavedKills = TotalKills;
	SaveInstance->SavedMementos = TotalMementos;
	SaveInstance->SaveSlotName = SlotName;

	//파일로 저장
	UGameplayStatics::SaveGameToSlot(SaveInstance, SaveInstance->SaveSlotName, SaveInstance->UserIndex);
	UE_LOG(LogTemp, Warning, TEXT("Game Saved Successfully!"));
}

void UOblivioGameInstance::LoadGameData(FString SlotName)
{

	// 세이브 파일이 존재하는지 확인
	if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		USaveGame* LoadedData = UGameplayStatics::LoadGameFromSlot(SlotName, 0);

		UOblivioSaveGame* LoadInstance = Cast<UOblivioSaveGame>(LoadedData);

		// 파일 데이터를 다시 GameInstance 변수로 복구
		SavedInventorySlots = LoadInstance->SavedInventorySlots;
		CurrentHealth = LoadInstance->SavedCurrentHealth;
		CurrentBattery = LoadInstance->SavedBattery;
		CurrentHunger = LoadInstance->SavedCurrentHunger;
		CurrentThirst = LoadInstance->SavedCurrentThirst;
		CurrentFloor = LoadInstance->SavedFloor;
		TotalKills = LoadInstance->SavedKills;
		TotalMementos = LoadInstance->SavedMementos;

		UE_LOG(LogTemp, Warning, TEXT("Game Loaded Successfully!"));
	}
}