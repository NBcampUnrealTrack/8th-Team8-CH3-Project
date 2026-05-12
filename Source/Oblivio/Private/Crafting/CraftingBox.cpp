#include "Crafting/CraftingBox.h"
#include "Components/StaticMeshComponent.h"

ACraftingBox::ACraftingBox()
{
	// 기본 스탯 설정 — 에너미 근접은 1 데미지/타이밍이므로 MaxHealth = 맞아야 할 대 수
	MaxHealth = 5.0f;
	CurrentHealth = MaxHealth;
	WoodCost = 2;
	IronCost = 0;

	// 내비게이션 영향을 주어 몬스터가 길을 돌아가게 만듦
	MeshComponent->SetCanEverAffectNavigation(true);
}

void ACraftingBox::BeginPlay()
{
	Super::BeginPlay();
}

void ACraftingBox::OnPlaced()
{
	Super::OnPlaced();

	// 설치시 소리 이벤트
}