#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Items/OblivioInventoryComponent.h"
#include "OblivioSaveGame.generated.h"

UCLASS()
class OBLIVIO_API UOblivioSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	//저장할 데이터들(낡은 저장·부분 초기화에서 0 피 피격 방어)
	UPROPERTY()
	float SavedCurrentHealth = 100.f;
	UPROPERTY()
	float SavedBattery = 100.f;
	UPROPERTY()
	float SavedCurrentHunger = 100.f;
	UPROPERTY()
	float SavedCurrentThirst = 100.f;
	UPROPERTY()
	int32 SavedFloor = 9;
	UPROPERTY()
	int32 SavedKills = 0;
	UPROPERTY()
	int32 SavedMementos = 0;
	UPROPERTY()
	bool SavedMementoEyeCollected = false;
	UPROPERTY()
	TArray<FInventorySlot> SavedInventorySlots;

	//세이브 슬롯 이름 및 인덱스
	UPROPERTY()
	FString SaveSlotName = TEXT("Oblivio_SaveSlot_0");
	UPROPERTY()
	uint32 UserIndex = 0;
};