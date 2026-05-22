#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "BestiaryData.generated.h"

USTRUCT(BlueprintType)
struct FBestiaryData : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bestiary")
    FName MonsterName; // 예: "The Limping Chaser"

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bestiary")
    FString CaseID; // 예: "[Case: R-02_LIMP]"

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bestiary")
    FString ThreatLevel; // 예: "High ****"

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bestiary")
    FText Behavior;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bestiary")
    FText Weakness;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bestiary")
    FText ObservationRecord;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bestiary")
    class UTexture2D* MonsterImage; // 몬스터 일러스트 또는 실루엣
};
