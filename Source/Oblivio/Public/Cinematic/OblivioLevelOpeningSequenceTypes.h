#pragma once

#include "CoreMinimal.h"
#include "OblivioLevelOpeningSequenceTypes.generated.h"

class ULevelSequence;

/** 레벨(맵) 이름별 오프닝 Level Sequence. Level Sequence가 비어 있으면 평소 ABP 동작. */
USTRUCT(BlueprintType)
struct FLevelOpeningSequenceEntry
{
	GENERATED_BODY()

	/** GetCurrentLevelName(World, true) 와 동일한 짧은 맵 이름. 예: L_Floor9_DoctorsLounge */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cinematic")
	FName LevelName = NAME_None;

	/** 비어 있으면 해당 맵은 시퀀스 없음 — Prepare/Restore 호출 안 함. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cinematic")
	TSoftObjectPtr<ULevelSequence> LevelSequence;
};
