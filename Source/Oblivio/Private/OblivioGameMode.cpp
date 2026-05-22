#include "OblivioGameMode.h"
#include "OblivioCharacter.h"
#include "OblivioCharacterController.h"
#include "OblivioGameInstance.h"
#include "Memento/FloodLevelActor.h"
#include "Kismet/GameplayStatics.h"

AOblivioGameMode::AOblivioGameMode()
{
	DefaultPawnClass = AOblivioCharacter::StaticClass();
	PlayerControllerClass = AOblivioCharacterController::StaticClass();
}

void AOblivioGameMode::BeginPlay()
{
	Super::BeginPlay();
}

void AOblivioGameMode::NextFloor()
{
	UOblivioGameInstance* GI = Cast<UOblivioGameInstance>(GetGameInstance());
	AOblivioCharacter* Player = Cast<AOblivioCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));

	if (!GI || !Player) return;

	if (CollectedKeys >= RequiredKeys)
	{
		//캐릭터의 현재 스탯을 인스턴스에 백업
		GI->CurrentHealth = Player->CurrentHealth;
		GI->CurrentBattery = Player->Battery;
		GI->CurrentHunger = Player->Hunger;
		GI->CurrentThirst = Player->Thirst;

		GI->CurrentFloor--;

		//다음 층 진행 시 자동 세이브할 경우
		//GI->SaveGameData();

		if (GI->CurrentFloor <= 1)
		{
			DetermineEnding();
		}
		else if(GI->FloorMapNames.Contains(GI->CurrentFloor))
		{
			FName NextLevelName = GI->FloorMapNames[GI->CurrentFloor];
			UGameplayStatics::OpenLevel(this, NextLevelName);
		}
		else
		{
			// 에러 처리: 등록된 맵이 없을 경우
			UE_LOG(LogTemp, Warning, TEXT("Floor is not exist: %d"), GI->CurrentFloor);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Not enough keys!"));
	}
}

// 체크포인트에서 상호작용 시 세이브할 경우
void AOblivioGameMode::RestInteraction()
{
	UOblivioGameInstance* GI = Cast<UOblivioGameInstance>(GetGameInstance());
	GI->bIsSaveMode = true;
	OpenSaveMenuUI();
}

/* //메뉴 진입해서 수동 세이브할 경우
void AOblivioGameMode::ManualSaveFromMenu()
{
	if (UOblivioGameInstance* GI = Cast<UOblivioGameInstance>(GetGameInstance()))
	{
		// 플레이어가 메뉴에서 '저장' 버튼을 눌렀을 때 호출
		GI->SaveGameData();
	}
}
*/

void AOblivioGameMode::AddMonsterKill()
{
	if (UOblivioGameInstance* GI = Cast<UOblivioGameInstance>(GetGameInstance()))
	{
		GI->TotalKills++;
	}
}

void AOblivioGameMode::AddMemento()
{
	if (UOblivioGameInstance* GI = Cast<UOblivioGameInstance>(GetGameInstance()))
	{
		GI->TotalMementos++;
	}
}

EGameEndingType AOblivioGameMode::DetermineEnding()
{
	UOblivioGameInstance* GI = Cast<UOblivioGameInstance>(GetGameInstance());
	if (!GI) return EGameEndingType::None;

	EGameEndingType FinalEnding = EGameEndingType::None;

	if (GI->CurrentFloor == 0)
	{
		FinalEnding = EGameEndingType::Oblivion;
	}
	else if (GI->TotalMementos > 0)//수치 조정 필요
	{
		FinalEnding = EGameEndingType::DeathRow;
	}
	else
	{
		FinalEnding = EGameEndingType::InfiniteLoop; //기본 엔딩
	}

	//추가 로직 필요하면 이쪽에
	UE_LOG(LogTemp, Warning, TEXT("Final Ending Determined: %d"), (int32)FinalEnding);
	return FinalEnding;
}

void AOblivioGameMode::TriggerFloodEvent()
{
	if (!ActiveFloodActor)
	{
		ActiveFloodActor = Cast<AFloodLevelActor>(UGameplayStatics::GetActorOfClass(GetWorld(), AFloodLevelActor::StaticClass()));
	}

	if (ActiveFloodActor)
	{
		ActiveFloodActor->StartFloodEvent();

		GetWorldTimerManager().SetTimer(FloodTimerHandle, this, &AOblivioGameMode::HandleFloodTimeout, 60.0f, false);

		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("비탄의 눈물이 차오릅니다! 1분 안에 탈출하세요!"));
	}
}
void AOblivioGameMode::HandleFloodTimeout()
{
	AOblivioCharacter* Player = Cast<AOblivioCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));

	if (Player && Player->IsAlive())
	{
		Player->ApplyHealth(Player->MaxHealth + 100.0f);

		UE_LOG(LogTemp, Warning, TEXT("Flood Event Failed: Player drowned in tears."));
	}

	if (ActiveFloodActor)
	{
		ActiveFloodActor->StopFloodEffects();
	}
}

void AOblivioGameMode::GameOver()
{
	UOblivioGameInstance* GI = Cast<UOblivioGameInstance>(GetGameInstance());
	if (!GI) return;

	GI->ResetGameData();

	UE_LOG(LogTemp, Warning, TEXT("Game Over!"));
}