#include "DoorBase.h"
#include "Kismet/GameplayStatics.h"
#include "OblivioGameInstance.h"
#include "OblivioCharacter.h"

ADoorBase::ADoorBase()
{
	PrimaryActorTick.bCanEverTick = false;

}
void ADoorBase::InteractDoor_Implementation()
{
	// 다음 레벨로 넘어가는 특수 출구 문일 경우
	if (bIsExitDoor)
	{
		UOblivioGameInstance* GI = Cast<UOblivioGameInstance>(UGameplayStatics::GetGameInstance(GetWorld()));
		AOblivioCharacter* Player = Cast<AOblivioCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));

		if (GI && Player)
		{
			// 플레이어의 상태 다음 층에서도 유지
			GI->CurrentHealth = Player->CurrentHealth;
			GI->CurrentBattery = Player->Battery;
			GI->CurrentHunger = Player->Hunger;
			GI->CurrentThirst = Player->Thirst;

			GI->CurrentFloor += 1;

			GI->SaveGameData();

			if (!NextLevelName.IsNone())
			{
				UGameplayStatics::OpenLevel(this, NextLevelName);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("Exit Door Error: NextLevelName is missing on %s!"), *GetName());
			}
		}
	}
	else
	{
		// 일반 문일 경우
		UE_LOG(LogTemp, Log, TEXT("Normal door interacted."));
	}
}

