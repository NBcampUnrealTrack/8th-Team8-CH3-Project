#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Items/OblivioItemBase.h"
#include "OblivioInventoryComponent.generated.h"

/** 인벤토리 한 칸의 정보를 담는 구조체 */
USTRUCT(BlueprintType)
struct FInventorySlot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName ItemID = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText ItemName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	class UTexture2D* ItemIcon = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Quantity = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxStack = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EItemType ItemType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class AOblivioItemBase> ItemClass;

	// 슬롯이 비어있는지 확인
	bool IsEmpty() const { return ItemID.IsNone() || Quantity <= 0; }
};

/** UI 갱신을 위한 델리게이트 (인벤토리 내용이 변할 때마다 UI에 신호를 보냄) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryUpdated);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class OBLIVIO_API UOblivioInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOblivioInventoryComponent();

protected:
	virtual void BeginPlay() override;

public:
	// ==========================================
	// Core Data
	// ==========================================
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnInventoryUpdated OnInventoryUpdated;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	int32 InventorySize = 20;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	TArray<FInventorySlot> InventorySlots;

	// ==========================================
	// Core Logic
	// ==========================================

	/** 아이템 추가 (습득 시 호출) */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool AddItem(class AOblivioItemBase* Item);

	/** 아이템 사용 (UI에서 우클릭 등) */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void UseItem(int32 SlotIndex);

	/** 아이템 버리기 (UI에서 밖으로 드래그 등) */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void DropItem(int32 SlotIndex);

	/** 아이템 위치 변경 (UI 드래그 앤 드롭) */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SwapSlots(int32 FromIndex, int32 ToIndex);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool HasItem(FName SearchItemID) const;

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	int32 GetItemCount(EItemType Type) const;

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool ConsumeItem(EItemType Type, int32 Amount);

private:
	/** 비어있는 슬롯 찾기 */
	int32 FindEmptySlot();

	/** 같은 아이템이 있고 중첩 가능한지 확인 */
	int32 FindStackableSlot(FName ID);
		
};
