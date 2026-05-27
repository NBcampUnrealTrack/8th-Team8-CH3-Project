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

	/** 메인 메뉴 이동 시 BP에서 ResetGameData 대신 호출 — 연출 에너미 처치·손전등·오프닝 LS 기록 유지. */
	UFUNCTION(BlueprintCallable, Category = "SaveSystem")
	void PreserveSessionStateForMainMenuReturn();

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
		bFlashlightWorldPickupCollected = false;
		PlayedOpeningLevelSequenceLevels.Empty();
		DefeatedStagingEnemyKeys.Empty();
		DefeatedStagingEnemyLevels.Empty();
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

	/** 9층 책상 등 월드 손전등 픽업(E)을 이미 획득했으면 true — 픽업 액터·VFX 재표시 방지. */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "PlayerStatus|Flashlight")
	bool bFlashlightWorldPickupCollected = false;

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

	/** 처치한 연출(Staging) 에너미 키 — 맵 재입장·메인메뉴 복귀 시 재스폰 방지. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Progress|Staging")
	TArray<FName> DefeatedStagingEnemyKeys;

	/** 처치 완료된 맵 — 액터 이름과 무관하게 해당 맵의 연출 에너미 전부 제거. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Progress|Staging")
	TArray<FName> DefeatedStagingEnemyLevels;

	UFUNCTION(BlueprintPure, Category = "Progress|Staging")
	bool IsStagingEnemyDefeated(FName PersistenceKey) const;

	UFUNCTION(BlueprintCallable, Category = "Progress|Staging")
	void MarkStagingEnemyDefeated(FName PersistenceKey);

	UFUNCTION(BlueprintPure, Category = "Progress|Staging")
	bool IsStagingEnemyDefeatedForLevel(FName LevelName) const;

	UFUNCTION(BlueprintCallable, Category = "Progress|Staging")
	void MarkStagingEnemyDefeatedForLevel(FName LevelName);

	UFUNCTION(BlueprintCallable, Category = "SaveSystem")
	void SaveSessionPersistence();

	UFUNCTION(BlueprintCallable, Category = "SaveSystem")
	void LoadSessionPersistence();

	UFUNCTION(BlueprintCallable, Category = "SaveSystem")
	void ClearSessionPersistence();

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

	/**
	 * 플레이어 사망 직후 설정 — Continue 로 같은 층을 다시 열 때 탱커 어그로·조우 울타리가
	 * 즉시 재활성화되지 않도록 GameMode BeginPlay 에서 1회 초기화한다.
	 */
	UPROPERTY(Transient, BlueprintReadWrite, Category = "Progress|Tank")
	bool bResetTankEncounterOnNextLevelLoad = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FString CurrentLanguage = TEXT("en"); // 기본값 설정

	UFUNCTION(BlueprintCallable, Category = "Settings")
	void SetLanguage(FString NewLanguage);

	UFUNCTION(BlueprintCallable, Category = "Settings")
	FString GetCurrentLanguage() const;
};