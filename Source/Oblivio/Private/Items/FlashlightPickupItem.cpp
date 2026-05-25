#include "Items/FlashlightPickupItem.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "NiagaraComponent.h"
#include "OblivioCharacter.h"

AFlashlightPickupItem::AFlashlightPickupItem()
{
	Tags.Add(FName(TEXT("Flashlight")));

	ItemCategory = EItemCategory::Special;
	ItemName = NSLOCTEXT("FlashlightPickup", "ItemName", "Flashlight");
	ItemDescription = NSLOCTEXT("FlashlightPickup", "ItemDescription", "Pick up the flashlight.");
	InteractText = NSLOCTEXT("FlashlightPickup", "InteractText", "Pick Up");
	MaxQuantity = 1;
	CurrentQuantity = 1;
}

void AFlashlightPickupItem::BeginPlay()
{
	Super::BeginPlay();
	SetPickupInteractable(!bHiddenUntilOpeningCinematicEnds, true);
}

void AFlashlightPickupItem::SetPickupInteractable(bool bInteractable, bool bForce)
{
	if (!bForce && bPickupInteractable == bInteractable)
	{
		return;
	}

	if (!bInteractable)
	{
		ClearOverlappingPlayers();
	}

	bPickupInteractable = bInteractable;

	// 시네마틱 연출용으로 메시는 항상 표시. 픽업·충돌·VFX만 토글한다.
	SetActorHiddenInGame(false);

	if (ItemMesh)
	{
		ItemMesh->SetHiddenInGame(false);
		ItemMesh->SetVisibility(true);
		ItemMesh->SetCollisionEnabled(
			bInteractable ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	}

	SetActorEnableCollision(bInteractable);

	if (InteractionSphere)
	{
		InteractionSphere->SetCollisionEnabled(
			bInteractable ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		InteractionSphere->SetGenerateOverlapEvents(bInteractable);
	}

	if (LootNiagaraComponent)
	{
		LootNiagaraComponent->SetHiddenInGame(!bInteractable);
		if (!bInteractable)
		{
			LootNiagaraComponent->Deactivate();
		}
	}

	if (bInteractable)
	{
		RefreshOverlappingPlayers();
	}
}

void AFlashlightPickupItem::RefreshOverlappingPlayers()
{
	if (!bPickupInteractable || !InteractionSphere)
	{
		return;
	}

	InteractionSphere->UpdateOverlaps();

	TArray<AActor*> OverlappingActors;
	InteractionSphere->GetOverlappingActors(OverlappingActors, AOblivioCharacter::StaticClass());
	for (AActor* Actor : OverlappingActors)
	{
		NotifyPlayerNearbyPickup(Cast<AOblivioCharacter>(Actor), true);
	}
}

void AFlashlightPickupItem::ClearOverlappingPlayers()
{
	if (!InteractionSphere)
	{
		return;
	}

	TArray<AActor*> OverlappingActors;
	InteractionSphere->GetOverlappingActors(OverlappingActors, AOblivioCharacter::StaticClass());
	for (AActor* Actor : OverlappingActors)
	{
		NotifyPlayerNearbyPickup(Cast<AOblivioCharacter>(Actor), false);
	}
}

void AFlashlightPickupItem::SetPickupCollisionEnabled(bool bEnabled)
{
	if (!bEnabled)
	{
		ClearOverlappingPlayers();

		if (InteractionSphere)
		{
			InteractionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			InteractionSphere->SetGenerateOverlapEvents(false);
		}
		return;
	}

	if (!bPickupInteractable)
	{
		return;
	}

	if (InteractionSphere)
	{
		InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		InteractionSphere->SetGenerateOverlapEvents(true);
	}

	RefreshOverlappingPlayers();
}
