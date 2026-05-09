#include "Items/OblivioInventoryComponent.h"
#include "Items/OblivioItemBase.h"
#include "OblivioCharacter.h"

UOblivioInventoryComponent::UOblivioInventoryComponent()
{

	PrimaryComponentTick.bCanEverTick = false;

}


// Called when the game starts
void UOblivioInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	InventorySlots.SetNum(InventorySize);
	
}

bool UOblivioInventoryComponent::HasItem(FName SearchItemID) const
{
	// 인벤토리 슬롯을 처음부터 끝까지 검사
	for (const FInventorySlot& Slot : InventorySlots)
	{
		if (!Slot.IsEmpty() && Slot.ItemID == SearchItemID)
		{
			return true;
		}
	}
	return false;
}

bool UOblivioInventoryComponent::AddItem(AOblivioItemBase* Item)
{
	if (!Item) return false;

	// 1. 기존에 겹칠 수 있는 슬롯이 있는지 확인
	int32 TargetSlot = FindStackableSlot(Item->ItemID);

	if (TargetSlot == INDEX_NONE)
	{
		// 2. 겹칠 곳이 없으면 새 빈 공간 찾기
		TargetSlot = FindEmptySlot();
	}

	if (TargetSlot != INDEX_NONE)
	{
		FInventorySlot& Slot = InventorySlots[TargetSlot];

		// 처음 들어가는 아이템이면 데이터 복사
		if (Slot.IsEmpty())
		{
			Slot.ItemID = Item->ItemID;
			Slot.ItemName = Item->ItemName;
			Slot.ItemIcon = Item->ItemIcon;
			Slot.MaxStack = Item->MaxQuantity;
			Slot.ItemType = Item->ItemType;
			Slot.ItemClass = Item->GetClass();
			Slot.Quantity = Item->CurrentQuantity;
		}
		else
		{
			// 이미 있으면 수량만 추가
			Slot.Quantity = FMath::Clamp(Slot.Quantity + Item->CurrentQuantity, 0, Slot.MaxStack);
		}

		OnInventoryUpdated.Broadcast(); // UI에 알림
		return true;
	}

	return false;
}

void UOblivioInventoryComponent::UseItem(int32 SlotIndex)
{
	if (!InventorySlots.IsValidIndex(SlotIndex) || InventorySlots[SlotIndex].IsEmpty()) return;

	FInventorySlot& Slot = InventorySlots[SlotIndex];
	AOblivioCharacter* Player = Cast<AOblivioCharacter>(GetOwner());

	if (Player)
	{
		if (Slot.ItemType == EItemType::Food) Player->Hunger = FMath::Min(100.0f, Player->Hunger + 30.0f);
		if (Slot.ItemType == EItemType::Water) Player->Thirst = FMath::Min(100.0f, Player->Thirst + 20.0f);

		if (Slot.ItemType == EItemType::Key || Slot.ItemType == EItemType::Memento)
		{
			// (나중에 일기장 UI 띄우기)
			return;
		}

		// 수량 감소
		Slot.Quantity--;
		if (Slot.Quantity <= 0) Slot = FInventorySlot(); // 슬롯 초기화

		OnInventoryUpdated.Broadcast();
	}
}

void UOblivioInventoryComponent::DropItem(int32 SlotIndex)
{
	if (!InventorySlots.IsValidIndex(SlotIndex) || InventorySlots[SlotIndex].IsEmpty()) return;

	FInventorySlot& Slot = InventorySlots[SlotIndex];

	if (Slot.ItemClass)
	{
		FVector DropLoc = GetOwner()->GetActorLocation() + GetOwner()->GetActorForwardVector() * 100.0f;
		GetWorld()->SpawnActor<AOblivioItemBase>(Slot.ItemClass, DropLoc, FRotator::ZeroRotator);

		Slot = FInventorySlot(); // 슬롯 초기화
		OnInventoryUpdated.Broadcast();
	}
}

void UOblivioInventoryComponent::SwapSlots(int32 FromIndex, int32 ToIndex)
{
	if (InventorySlots.IsValidIndex(FromIndex) && InventorySlots.IsValidIndex(ToIndex))
	{
		InventorySlots.Swap(FromIndex, ToIndex);
		OnInventoryUpdated.Broadcast();
	}
}

int32 UOblivioInventoryComponent::FindEmptySlot()
{
	for (int32 i = 0; i < InventorySlots.Num(); ++i)
	{
		if (InventorySlots[i].IsEmpty()) return i;
	}
	return INDEX_NONE;
}

int32 UOblivioInventoryComponent::FindStackableSlot(FName ID)
{
	for (int32 i = 0; i < InventorySlots.Num(); ++i)
	{
		if (InventorySlots[i].ItemID == ID && InventorySlots[i].Quantity < InventorySlots[i].MaxStack)
		{
			return i;
		}
	}
	return INDEX_NONE;
}
