#include "Crafting/CraftingWall.h"
#include "Components/StaticMeshComponent.h"

ACraftingWall::ACraftingWall()
{
	MaxHealth = 7.0f;
	CurrentHealth = MaxHealth;

    // 설치 비용 설정
    WoodCost = 4;
    IronCost = 0;

    // 몬스터가 이 벽을 피해가도록 내비게이션 메시 리빌드 설정
    if (MeshComponent)
    {
        MeshComponent->SetCanEverAffectNavigation(true);
    }
}

void ACraftingWall::BeginPlay()
{
    Super::BeginPlay();
}

void ACraftingWall::OnPlaced()
{
    Super::OnPlaced();
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Orange, TEXT("Wall Construction Completed!"));
    }
}
