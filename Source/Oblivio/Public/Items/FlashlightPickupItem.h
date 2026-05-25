#pragma once

#include "CoreMinimal.h"
#include "Items/OblivioItemBase.h"
#include "FlashlightPickupItem.generated.h"

/** 월드에 배치해 E키로 획득하는 손전등. 오프닝 시네마틱 중에는 메시만 보이고, 종료 후 E 픽업 가능. */
UCLASS(Blueprintable)
class OBLIVIO_API AFlashlightPickupItem : public AOblivioItemBase
{
	GENERATED_BODY()

public:
	AFlashlightPickupItem();

	UFUNCTION(BlueprintCallable, Category = "Pickup")
	void SetPickupInteractable(bool bInteractable, bool bForce = false);

	UFUNCTION(BlueprintPure, Category = "Pickup")
	bool IsPickupInteractable() const { return bPickupInteractable; }

	virtual void SetPickupCollisionEnabled(bool bEnabled) override;

protected:
	virtual void BeginPlay() override;
	virtual bool CanShowNearbyPickupUI() const override { return bPickupInteractable; }

	void RefreshOverlappingPlayers();
	void ClearOverlappingPlayers();

	/** true면 오프닝 Level Sequence 종료(또는 스킵) 전까지 E 픽업·오버랩만 비활성. 메시는 시네마틱용으로 계속 표시. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup")
	bool bHiddenUntilOpeningCinematicEnds = true;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Pickup")
	bool bPickupInteractable = false;
};
