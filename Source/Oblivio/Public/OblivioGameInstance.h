#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Cinematic/OblivioLevelOpeningSequenceTypes.h"
#include "Items/OblivioInventoryComponent.h"
#include "OblivioGameInstance.generated.h"

class ULevelSequence;
class UWorld;

UCLASS()
class OBLIVIO_API UOblivioGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	UOblivioGameInstance();

	//게임 재시작 시 데이터 초기화용
	void ResetGameData()
	{
		TotalKills = 0;
		TotalMementos = 0;
		CurrentFloor = 9;		
		SavedInventorySlots.Empty();
		CurrentHealth = 100.f;
		CurrentBattery = 100.f;
		CurrentHunger = 100.f;
		CurrentThirst = 100.f;
		bMementoEyeCollected = false;
		bFlashlightAcquired = false;
		bFlashlightOn = false;
		PlayedOpeningLevelSequenceLevels.Empty();
		UnlockedMonsters.Empty();
	}

	//세이브/로드 함수
	UFUNCTION(BlueprintCallable, Category = "SaveSystem")
	void SaveGameData(FString SlotName);
	UFUNCTION(BlueprintCallable, Category = "SaveSystem")
	void LoadGameData(FString SlotName);

	//게임 도중 계속 유지되어야 하는 자원 정보
	//인벤토리
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Inventory")
	TArray<FInventorySlot> SavedInventorySlots;

	//플레이어의 현재 스탯
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "PlayerStatus")
	float CurrentHealth = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "PlayerStatus")
	float CurrentBattery = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "PlayerStatus")
	float CurrentHunger = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "PlayerStatus")
	float CurrentThirst = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "PlayerStatus|Flashlight")
	bool bFlashlightAcquired = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "PlayerStatus|Flashlight")
	bool bFlashlightOn = false;

	//몬스터 도감
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Persistence|Bestiary")
	TArray<FName> UnlockedMonsters;

	// 도감 획득 시 호출할 함수
	UFUNCTION(BlueprintCallable, Category = "Persistence|Bestiary")
	void UnlockMonsterEntry(FName MonsterID);

	// 특정 몬스터가 해금되었는지 검사하는 함수 (UI에서 체크할 때 유용함)
	UFUNCTION(BlueprintPure, Category = "Persistence|Bestiary")
	bool IsMonsterUnlocked(FName MonsterID) const;

	//카르마/유품/층수
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Persistence")
	int32 TotalKills = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Persistence")
	int32 TotalMementos = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Persistence")
	int32 CurrentFloor = 9;

	/** 맵 이름별 오프닝 Level Sequence (GameInstance BP Class Defaults에서 편집). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cinematic")
	TArray<FLevelOpeningSequenceEntry> LevelOpeningSequenceEntries;

	/** 이미 재생 완료한 맵의 오프닝 LS (세이브 연동). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cinematic")
	TArray<FName> PlayedOpeningLevelSequenceLevels;

	/** 현재 월드 맵 이름에 맞는 Level Sequence. 없거나 비어 있으면 nullptr. */
	ULevelSequence* ResolveOpeningLevelSequence(const UWorld* World) const;

	/** 현재 맵에 재생할 시퀀스가 있는지 (에셋까지 유효). */
	UFUNCTION(BlueprintPure, Category = "Cinematic")
	bool HasOpeningLevelSequenceForCurrentLevel(const UWorld* World) const;

	/** 해당 맵 오프닝 LS를 이미 재생했는지 (재입장·다시하기 시 스킵). */
	UFUNCTION(BlueprintPure, Category = "Cinematic")
	bool HasPlayedOpeningLevelSequence(const UWorld* World) const;

	UFUNCTION(BlueprintCallable, Category = "Cinematic")
	void MarkOpeningLevelSequencePlayed(const UWorld* World);

	/** `ItemID == MementoEye` 메멘토 획득 시 true — 7층 추적 눈알 등 게이트용. */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Persistence|Memento")
	bool bMementoEyeCollected = false;

	//레벨 이름 저장하는 맵
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Game Data")
	TMap<int32, FName> FloorMapNames;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	bool bIsSaveMode = false;

	// -------------------------------------------------------------------------
	// 탱커 첫 조우 — 럭스이터 양막 소환 패턴 게이트(세션·세이브 연동)
	// -------------------------------------------------------------------------
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Progress|Tank")
	bool bPlayerMetTankOnce = false;

	UFUNCTION(BlueprintCallable, Category = "Progress|Tank")
	void MarkPlayerMetTankOnce();

	UFUNCTION(BlueprintPure, Category = "Progress|Tank")
	bool HasPlayerMetTankOnce() const { return bPlayerMetTankOnce; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FString CurrentLanguage = TEXT("en"); // 기본값 설정

	UFUNCTION(BlueprintCallable, Category = "Settings")
	void SetLanguage(FString NewLanguage);

	UFUNCTION(BlueprintCallable, Category = "Settings")
	FString GetCurrentLanguage() const;
};