#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "OblivioSaveGame.generated.h"

UCLASS()
class OBLIVIO_API UOblivioSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	//저장할 데이터들
	UPROPERTY()
	int32 SavedWoodCount;
	UPROPERTY()
	int32 SavedIronCount;
	UPROPERTY()
	int32 SavedBatteryItemCount;
	UPROPERTY()
	float SavedCurrentHealth;
	UPROPERTY()
	float SavedBattery;
	UPROPERTY()
	float SavedCurrentHunger;
	UPROPERTY()
	float SavedCurrentThirst;
	UPROPERTY()
	int32 SavedFloor;
	UPROPERTY()
	int32 SavedKills;
	UPROPERTY()
	int32 SavedMementos;

	//세이브 슬롯 이름 및 인덱스
	UPROPERTY()
	FString SaveSlotName = TEXT("Oblivio_SaveSlot_0");
	UPROPERTY()
	uint32 UserIndex = 0;
};