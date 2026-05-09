#include "Crafting/OblivioCrafting.h"
#include "Crafting/ObstacleBase.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

#include "OblivioCharacter.h"
#include "Items/OblivioInventoryComponent.h"
#include "Items/OblivioItemBase.h"

UOblivioCrafting::UOblivioCrafting()
{
	PrimaryComponentTick.bCanEverTick = true;
	bIsCraftingModeActive = false;
}

void UOblivioCrafting::BeginPlay()
{
	Super::BeginPlay();
}

void UOblivioCrafting::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bIsCraftingModeActive && PreviewActor)
	{
		UpdatePreviewLocation();
	}
}

void UOblivioCrafting::ToggleCraftingMode()
{
    bIsCraftingModeActive = !bIsCraftingModeActive;

    if (GEngine)
    {
        FString ModeMsg = FString::Printf(TEXT("Crafting Mode: %s"), bIsCraftingModeActive ? TEXT("ON") : TEXT("OFF"));
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, ModeMsg);
    }

    if (bIsCraftingModeActive)
    {
        if (SelectedObstacleClass)
        {
            // 프리뷰용 고스트 액터 생성
            FActorSpawnParameters SpawnParams;
            PreviewActor = GetWorld()->SpawnActor<AObstacleBase>(SelectedObstacleClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);

            if (PreviewActor)
            {
                PreviewActor->SetGhostMode(true);
                if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, TEXT("Ghost Spawned!"));
            }
        }
    }
    else
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("Error: SelectedObstacleClass is NONE!"));
        if (PreviewActor)
        {
            PreviewActor->Destroy();
            PreviewActor = nullptr;
        }
    }
}

void UOblivioCrafting::SelectObstacle(int32 Index)
{
    if (CraftingRecipes.Contains(Index))
    {
        SelectedObstacleClass = CraftingRecipes[Index];

            
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan, FString::Printf(TEXT("Item %d Selected"), Index));

        APlayerController* PC = Cast<APlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
        if (!PC) return;

        AObstacleBase* DefaultObj = SelectedObstacleClass->GetDefaultObject<AObstacleBase>();

        if (CanAfford(DefaultObj))
        {
            if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, TEXT("Step 2: Can Afford Success"));
            if (PreviewActor)
            {
                PreviewActor->Destroy();
                PreviewActor = nullptr;
            }

            bIsCraftingModeActive = true;
            CurrentPreviewRotation = FRotator::ZeroRotator;
            FHitResult TempHit;
            PC->GetHitResultUnderCursor(ECC_Visibility, false, TempHit);
            FVector SpawnLoc = TempHit.bBlockingHit ? TempHit.Location : GetOwner()->GetActorLocation();

            FActorSpawnParameters SpawnParams;
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn; 

            PreviewActor = GetWorld()->SpawnActor<AObstacleBase>(SelectedObstacleClass, SpawnLoc, FRotator::ZeroRotator, SpawnParams);

            if (PreviewActor)
            {
                PreviewActor->SetGhostMode(true);
                UpdatePreviewLocation();
                if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, TEXT("Ghost Spawned at Mouse!"));
            }
        }
        else // 재료가 부족하다면
        {
            if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("Error: Not Enough Resources!"));

            // 기존 고스트가 있었다면 제거
            if (PreviewActor)
            {
                PreviewActor->Destroy();
                PreviewActor = nullptr;
            }
            bIsCraftingModeActive = false;
        }
    }
    else
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, FString::Printf(TEXT("Error: Recipe Index %d not found!"), Index));
    }
}
void UOblivioCrafting::RotatePreview()
{
    if (!bIsCraftingModeActive) return;

    // Yaw 값을 90도씩 증가 (360도 도달 시 0으로)
    CurrentPreviewRotation.Yaw = FMath::Fmod(CurrentPreviewRotation.Yaw + 90.0f, 360.0f);

    if (PreviewActor)
    {
        PreviewActor->SetActorRotation(CurrentPreviewRotation);
    }
}

void UOblivioCrafting::UpdatePreviewLocation()
{
    APlayerController* PC = Cast<APlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
    if (!PC || !PreviewActor) return;

    FHitResult Hit;

    bool bHit = PC->GetHitResultUnderCursor(ECC_Visibility, false, Hit);

    if (bHit)
    {
        float Distance = FVector::Dist(GetOwner()->GetActorLocation(), Hit.Location);

        PreviewActor->SetActorLocation(Hit.Location);
        PreviewActor->SetActorRotation(CurrentPreviewRotation);

        if (Distance <= MaxPlacementDistance)
        {

            bool bCanPlace = CanAfford(PreviewActor);
            PreviewActor->SetGhostMode(true, bCanPlace);
        }
        else
        {
            PreviewActor->SetActorLocation(Hit.Location);
            PreviewActor->SetGhostMode(true, false);
        }
    }
}

void UOblivioCrafting::PlaceObstacle()
{
    if (bIsCraftingModeActive && PreviewActor)
    {
        // 최종 자원 소모 로직 적용 후 설치 확정
        if (CanAfford(PreviewActor))
        {
            PreviewActor->OnPlaced();
            PreviewActor = nullptr; // 소유권 해제
            bIsCraftingModeActive = false; // 설치 후 모드 종료
        }
    }
}
/*
void UOblivioCrafting::PlaceObstacle()
{
    if (bIsCraftingModeActive && PreviewActor)
    {
        // 최종 자원 소모 로직 적용 후 설치 확정
        if (CanAfford(PreviewActor))
        {
            // 인벤토리에서 자원을 차감
            AOblivioCharacter* Player = Cast<AOblivioCharacter>(GetOwner());
            if (Player && Player->InventoryComponent)
            {
                // 인벤토리 컴포넌트의 자원 소모 함수 호출
                Player->InventoryComponent->ConsumeItem(EItemType::Wood, PreviewActor->WoodCost);
                Player->InventoryComponent->ConsumeItem(EItemType::Iron, PreviewActor->IronCost);

                if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Orange, TEXT("Resources Consumed!"));
            }

            PreviewActor->OnPlaced();
            PreviewActor = nullptr; // 소유권 해제
            bIsCraftingModeActive = false; // 설치 후 모드 종료
        }
        else
        {
            if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("Cannot Place: Not enough resources!"));
        }
    }
}
*/
bool UOblivioCrafting::CanAfford(AObstacleBase* TargetObstacle)
{
    // Character 클래스나 Inventory 시스템에서 자원값을 가져와 비교
    return true;
}
/*
bool UOblivioCrafting::CanAfford(AObstacleBase* TargetObstacle)
{
    if (!TargetObstacle) return false;

    //인벤토리 가져오기
    AOblivioCharacter* Player = Cast<AOblivioCharacter>(GetOwner());
    if (!Player || !Player->InventoryComponent) return false;

    // 장애물 필요 자원 가져오기
    int32 RequiredWood = TargetObstacle->WoodCost;
    int32 RequiredIron = TargetObstacle->IronCost;

    // 현재 보유 중인 개수 확인 
    int32 CurrentWood = Player->InventoryComponent->GetItemCount(EItemType::Wood);
    int32 CurrentIron = Player->InventoryComponent->GetItemCount(EItemType::Iron);

    return (CurrentWood >= RequiredWood) && (CurrentIron >= RequiredIron);
}
*/