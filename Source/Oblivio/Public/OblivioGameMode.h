#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "OblivioGameMode.generated.h"

UENUM(BlueprintType)
enum class EGameEndingType : uint8
{
	None,
	DeathRow,    // 사형수
	InfiniteLoop, // 무한의 굴레
	Oblivion      // 안식의 망각
};

UCLASS()
class OBLIVIO_API AOblivioGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AOblivioGameMode();

protected:
	virtual void BeginPlay() override;

public:
	//다음 층 열쇠
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level")
	int32 CollectedKeys = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level")
	int32 RequiredKeys = 3;
	// ----

	//획득/소모한 아이템을 인스턴스로 넘겨주는 함수
	UFUNCTION(BlueprintCallable, Category = "Resource")
	void AddResource(FString Type, int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "Resource")
	bool ConsumeResource(int32 WoodCost, int32 IronCost);
	//---

	UFUNCTION(BlueprintCallable, Category = "Level")
	void NextFloor();

	UFUNCTION(BlueprintCallable, Category = "Level")
	void RestInteraction();

	UFUNCTION(BlueprintCallable, Category = "Karma")
	void AddMonsterKill();

	UFUNCTION(BlueprintCallable, Category = "Karma")
	void AddMemento();

	UFUNCTION(BlueprintCallable, Category = "Karma")
	EGameEndingType DetermineEnding();

	UFUNCTION(BlueprintCallable, Category = "Game State")
	void GameOver();
};