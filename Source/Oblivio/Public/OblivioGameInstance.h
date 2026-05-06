#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "OblivioGameInstance.generated.h"

UCLASS()
class OBLIVIO_API UOblivioGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:

	//게임 재시작 시 데이터 초기화용
	void ResetGameData()
	{
		TotalKills = 0;
		TotalMementos = 0;
		CurrentFloor = 9;		
		WoodCount = 0; 
		IronCount = 0;
		BatteryItemCount = 0;
		CurrentHealth = 100.f;
		CurrentBattery = 100.f;
		CurrentHunger = 100.f;
		CurrentThirst = 100.f;
	}

	//세이브/로드 함수
	void SaveGameData();
	void LoadGameData();

	//게임 도중 계속 유지되어야 하는 자원 정보
	//자원/인벤토리
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Inventory")
	int32 WoodCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Inventory")
	int32 IronCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "PlayerStatus")
	int32 BatteryItemCount = 0;

	//플레이어의 현재 스탯
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "PlayerStatus")
	float CurrentHealth = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "PlayerStatus")
	float CurrentBattery = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "PlayerStatus")
	float CurrentHunger = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "PlayerStatus")
	float CurrentThirst = 100.0f;

	//카르마/유품/층수
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Persistence")
	int32 TotalKills = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Persistence")
	int32 TotalMementos = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Persistence")
	int32 CurrentFloor = 9;
};