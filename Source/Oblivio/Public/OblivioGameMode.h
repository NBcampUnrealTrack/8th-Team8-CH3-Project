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
	int32 RequiredKeys = 1;
	// ----

	UFUNCTION(BlueprintCallable, Category = "Level")
	void NextFloor();

	UFUNCTION(BlueprintCallable, Category = "Level")
	void RestInteraction();

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "UI")
	void OpenSaveMenuUI();

	UFUNCTION(BlueprintCallable, Category = "Karma")
	void AddMonsterKill();

	UFUNCTION(BlueprintCallable, Category = "Karma")
	void AddMemento();

	UFUNCTION(BlueprintCallable, Category = "Karma")
	EGameEndingType DetermineEnding();

	UFUNCTION(BlueprintCallable, Category = "Game State")
	void GameOver();

	/** 홍수 이벤트를 트리거하는 함수 (메멘토 상호작용 시 호출) */
	UFUNCTION(BlueprintCallable, Category = "Level|Flood")
	void TriggerFloodEvent();
	UFUNCTION(BlueprintCallable, Category = "Level|Flood")
	void HandleFloodTimeout();

private:
	FTimerHandle FloodTimerHandle;

	/** 월드에 배치된 홍수 제어 액터 참조 */
	UPROPERTY()
	class AFloodLevelActor* ActiveFloodActor;
};