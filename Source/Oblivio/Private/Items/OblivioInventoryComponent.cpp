#include "Items/OblivioInventoryComponent.h"
#include "Items/OblivioItemBase.h"
#include "OblivioCharacter.h"
#include "OblivioGameInstance.h"
#include "Kismet/GameplayStatics.h"

UOblivioInventoryComponent::UOblivioInventoryComponent()
{

	PrimaryComponentTick.bCanEverTick = false;

}


// Called when the game starts
void UOblivioInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	InventorySlots.SetNum(InventorySize);

	//게임 시작 시 GameInstance에서 인벤토리 정보 로드
	if (UOblivioGameInstance* GI = Cast<UOblivioGameInstance>(UGameplayStatics::GetGameInstance(GetWorld())))
	{
		//저장된 인벤토리가 있다면 가져오고, 없으면 기본 크기로 세팅
		if (GI->SavedInventorySlots.Num() > 0)
		{
			InventorySlots = GI->SavedInventorySlots;
			OnInventoryUpdated.Broadcast();
		}
	}
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
			Slot.ItemDescription = Item->ItemDescription;
		}
		else
		{
			// 이미 있으면 수량만 추가
			Slot.Quantity = FMath::Clamp(Slot.Quantity + Item->CurrentQuantity, 0, Slot.MaxStack);
		}

		OnInventoryUpdated.Broadcast(); // UI에 알림
		SyncInventoryToGameInstance(); //인스턴스와 동기화
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
		float HungerRestore = 30.0f;
		float ThirstRestore = 20.0f;
		if (Slot.ItemClass)
		{
			const AOblivioItemBase* CDO = Slot.ItemClass.GetDefaultObject();
			if (IsValid(CDO))
			{
				if (Slot.ItemType == EItemType::Food && CDO->ItemType == EItemType::Food)
				{
					HungerRestore = CDO->RestoreValue;
				}
				else if (Slot.ItemType == EItemType::Water && CDO->ItemType == EItemType::Water)
				{
					ThirstRestore = CDO->RestoreValue;
				}
			}
		}

		if (Slot.ItemType == EItemType::Food)
		{
			Player->Hunger = FMath::Min(100.0f, Player->Hunger + HungerRestore);
		}
		if (Slot.ItemType == EItemType::Water)
		{
			Player->Thirst = FMath::Min(100.0f, Player->Thirst + ThirstRestore);
		}

		if (Slot.ItemType == EItemType::Key || Slot.ItemType == EItemType::Memento)
		{
			// (나중에 일기장 UI 띄우기)
			return;
		}

		// 수량 감소
		Slot.Quantity--;
		if (Slot.Quantity <= 0) Slot = FInventorySlot(); // 슬롯 초기화

		OnInventoryUpdated.Broadcast();
		SyncInventoryToGameInstance(); //인스턴스와 동기화
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

		Slot.Quantity--;
		if (Slot.Quantity <= 0) Slot = FInventorySlot(); // 슬롯 초기화
		OnInventoryUpdated.Broadcast();
		SyncInventoryToGameInstance(); //인스턴스와 동기화
	}
}

void UOblivioInventoryComponent::SwapSlots(int32 FromIndex, int32 ToIndex)
{
	if (InventorySlots.IsValidIndex(FromIndex) && InventorySlots.IsValidIndex(ToIndex))
	{
		InventorySlots.Swap(FromIndex, ToIndex);
		OnInventoryUpdated.Broadcast();
		SyncInventoryToGameInstance(); //인스턴스와 동기화
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

int32 UOblivioInventoryComponent::GetItemCount(EItemType Type) const
{
	int32 TotalCount = 0;

	for (const FInventorySlot& Slot : InventorySlots)
	{
		if (!Slot.IsEmpty() && Slot.ItemType == Type)
		{
			TotalCount += Slot.Quantity;
		}
	}

	return TotalCount;
}

bool UOblivioInventoryComponent::ConsumeItem(EItemType Type, int32 Amount)
{
	if (GetItemCount(Type) < Amount)
	{
		return false;
	}

	int32 RemainingToConsume = Amount;

	for (FInventorySlot& Slot : InventorySlots)
	{
		if (!Slot.IsEmpty() && Slot.ItemType == Type)
		{
			// 현재 슬롯의 개수가 빼야 할 개수보다 많거나 같으면
			if (Slot.Quantity >= RemainingToConsume)
			{
				Slot.Quantity -= RemainingToConsume;
				RemainingToConsume = 0;

				// 만약 슬롯이 텅 비게 되면 초기화
				if (Slot.Quantity <= 0)
				{
					Slot = FInventorySlot();
				}
				break; // 다 뺐으므로 루프 종료
			}
			else
			{
				// 현재 슬롯의 개수로는 부족하면 있는 거 다 빼고 다음 슬롯으로 넘어감
				RemainingToConsume -= Slot.Quantity;
				Slot = FInventorySlot(); // 슬롯 텅 빔
			}
		}
	}
	OnInventoryUpdated.Broadcast();
	SyncInventoryToGameInstance(); //인스턴스와 동기화
	return true;
}

//게임 인스턴스에 인벤토리 동기화 및 자동 저장
void UOblivioInventoryComponent::SyncInventoryToGameInstance()
{
	if (UOblivioGameInstance* GI = Cast<UOblivioGameInstance>(UGameplayStatics::GetGameInstance(GetWorld())))
	{
		GI->SavedInventorySlots = InventorySlots;
		// 음식/물 사용 등으로 캐릭터만 갱신되면 GI 가 남아 다음 BeginPlay/저장에서 덮어쓰기 됨
		if (AOblivioCharacter* const Player = Cast<AOblivioCharacter>(GetOwner()))
		{
			GI->CurrentHealth = Player->CurrentHealth;
			GI->CurrentBattery = Player->Battery;
			GI->CurrentHunger = Player->Hunger;
			GI->CurrentThirst = Player->Thirst;
		}
	}
}