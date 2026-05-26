#include "OblivioGameInstance.h"
#include "OblivioSaveGame.h"
#include "LevelSequence.h"
#include "Kismet/GameplayStatics.h"

UOblivioGameInstance::UOblivioGameInstance()
{
	FLevelOpeningSequenceEntry Floor9Entry;
	Floor9Entry.LevelName = FName(TEXT("L_Floor9_DoctorsLounge"));
	Floor9Entry.LevelSequence = TSoftObjectPtr<ULevelSequence>(
		FSoftObjectPath(TEXT("/Game/LevelSequence/LS_Foor9.LS_Foor9")));
	LevelOpeningSequenceEntries.Add(Floor9Entry);
}

ULevelSequence* UOblivioGameInstance::ResolveOpeningLevelSequence(const UWorld* World) const
{
	if (!World)
	{
		return nullptr;
	}

	const FName CurrentLevelName = FName(*UGameplayStatics::GetCurrentLevelName(World, true));
	if (CurrentLevelName.IsNone())
	{
		return nullptr;
	}

	for (const FLevelOpeningSequenceEntry& Entry : LevelOpeningSequenceEntries)
	{
		if (Entry.LevelName != CurrentLevelName)
		{
			continue;
		}

		if (Entry.LevelSequence.IsNull())
		{
			return nullptr;
		}

		return Entry.LevelSequence.LoadSynchronous();
	}

	return nullptr;
}

bool UOblivioGameInstance::HasOpeningLevelSequenceForCurrentLevel(const UWorld* World) const
{
	return ResolveOpeningLevelSequence(World) != nullptr;
}

bool UOblivioGameInstance::HasPlayedOpeningLevelSequence(const UWorld* World) const
{
	if (!World)
	{
		return false;
	}

	const FName CurrentLevelName = FName(*UGameplayStatics::GetCurrentLevelName(World, true));
	if (CurrentLevelName.IsNone())
	{
		return false;
	}

	return PlayedOpeningLevelSequenceLevels.Contains(CurrentLevelName);
}

void UOblivioGameInstance::MarkOpeningLevelSequencePlayed(const UWorld* World)
{
	if (!World)
	{
		return;
	}

	const FName CurrentLevelName = FName(*UGameplayStatics::GetCurrentLevelName(World, true));
	if (CurrentLevelName.IsNone())
	{
		return;
	}

	PlayedOpeningLevelSequenceLevels.AddUnique(CurrentLevelName);
}

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
	SaveInstance->SavedPlayedOpeningLevelSequences = PlayedOpeningLevelSequenceLevels;
	SaveInstance->SavedUnlockedMonsters = UnlockedMonsters;
	SaveInstance->SavedFlashlightAcquired = bFlashlightAcquired;
	SaveInstance->SavedFlashlightOn = bFlashlightOn;

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
	PlayedOpeningLevelSequenceLevels = LoadInstance->SavedPlayedOpeningLevelSequences;
	UnlockedMonsters = LoadInstance->SavedUnlockedMonsters;
	bFlashlightAcquired = LoadInstance->SavedFlashlightAcquired;
	bFlashlightOn = LoadInstance->SavedFlashlightOn;

	UE_LOG(LogTemp, Warning, TEXT("Game Loaded Successfully from '%s'."), *SlotName);
}

void UOblivioGameInstance::MarkPlayerMetTankOnce()
{
	bPlayerMetTankOnce = true;
}

void UOblivioGameInstance::UnlockMonsterEntry(FName MonsterID)
{
	// 이미 해금된 몬스터가 아니라면 추가
	if (!UnlockedMonsters.Contains(MonsterID))
	{
		UnlockedMonsters.AddUnique(MonsterID);
		UE_LOG(LogTemp, Warning, TEXT("Bestiary Unlocked: %s"), *MonsterID.ToString());
	}
}

bool UOblivioGameInstance::IsMonsterUnlocked(FName MonsterID) const
{
	return UnlockedMonsters.Contains(MonsterID);
}


void UOblivioGameInstance::SetLanguage(FString NewLanguage)
{
	CurrentLanguage = NewLanguage;
}

FString UOblivioGameInstance::GetCurrentLanguage() const
{
	return CurrentLanguage;
}