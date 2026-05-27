#include "Items/FlashlightPickupItem.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "NiagaraComponent.h"
#include "OblivioCharacter.h"
#include "OblivioGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Weapon/Flashlight.h"

namespace
{
	bool IsPlayerEquippedFlashlightActor(const AActor* Actor)
	{
		return IsValid(Actor) && Actor->IsA(AFlashlight::StaticClass());
	}

	bool IsFlashlightWorldPickupActor(const AActor* Actor)
	{
		if (!IsValid(Actor) || IsPlayerEquippedFlashlightActor(Actor))
		{
			return false;
		}

		if (Actor->IsA(AFlashlightPickupItem::StaticClass()))
		{
			return true;
		}

		static const FName FlashlightTag(TEXT("Flashlight"));
		static const FName FlashlightPickupTag(TEXT("FlashlightPickup"));
		if (Actor->ActorHasTag(FlashlightTag) || Actor->ActorHasTag(FlashlightPickupTag))
		{
			return true;
		}

		const FString ClassName = Actor->GetClass()->GetName();
		const FString ActorName = Actor->GetName();
		return ClassName.Contains(TEXT("FlashlightPickup"), ESearchCase::IgnoreCase)
			|| ActorName.Contains(TEXT("FlashlightPickup"), ESearchCase::IgnoreCase);
	}

	bool ShouldRemoveFlashlightWorldPickups(const UWorld* World, const UObject* WorldContextObject)
	{
		if (!World)
		{
			return false;
		}

		if (UOblivioGameInstance* GI = Cast<UOblivioGameInstance>(
				UGameplayStatics::GetGameInstance(WorldContextObject)))
		{
			GI->LoadSessionPersistence();
			if (GI->bFlashlightWorldPickupCollected || GI->bFlashlightAcquired)
			{
				return true;
			}
		}

		if (const AOblivioCharacter* Player =
				Cast<AOblivioCharacter>(UGameplayStatics::GetPlayerCharacter(World, 0)))
		{
			return Player->IsFlashlightAcquired() || Player->HasFlashlight();
		}

		return false;
	}

	void HideAndDestroyPickupActor(AActor* Actor)
	{
		if (!IsValid(Actor))
		{
			return;
		}

		if (AFlashlightPickupItem* Pickup = Cast<AFlashlightPickupItem>(Actor))
		{
			Pickup->HideAndDestroyWorldPickup();
			return;
		}

		Actor->SetActorHiddenInGame(true);
		Actor->SetActorEnableCollision(false);
		Actor->Destroy();
	}
}

AFlashlightPickupItem::AFlashlightPickupItem()
{
	Tags.Add(FName(TEXT("Flashlight")));
	Tags.Add(FName(TEXT("FlashlightPickup")));

	ItemCategory = EItemCategory::Special;
	ItemName = NSLOCTEXT("FlashlightPickup", "ItemName", "Flashlight");
	ItemDescription = NSLOCTEXT("FlashlightPickup", "ItemDescription", "Pick up the flashlight.");
	InteractText = NSLOCTEXT("FlashlightPickup", "InteractText", "Pick Up");
	MaxQuantity = 1;
	CurrentQuantity = 1;
}

void AFlashlightPickupItem::BeginPlay()
{
	if (ShouldRemoveFlashlightWorldPickups(GetWorld(), this))
	{
		HideAndDestroyWorldPickup();
		return;
	}

	Super::BeginPlay();
	SetPickupInteractable(!bHiddenUntilOpeningCinematicEnds, true);
}

void AFlashlightPickupItem::HideAndDestroyWorldPickup()
{
	ClearOverlappingPlayers();
	bPickupInteractable = false;

	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);

	if (ItemMesh)
	{
		ItemMesh->SetHiddenInGame(true);
		ItemMesh->SetVisibility(false);
		ItemMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (InteractionSphere)
	{
		InteractionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		InteractionSphere->SetGenerateOverlapEvents(false);
	}

	if (LootNiagaraComponent)
	{
		LootNiagaraComponent->SetHiddenInGame(true);
		LootNiagaraComponent->Deactivate();
	}

	Destroy();
}

void AFlashlightPickupItem::DestroyAllInWorldIfFlashlightAlreadyAcquired(const UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World || !ShouldRemoveFlashlightWorldPickups(World, WorldContextObject))
	{
		return;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (IsFlashlightWorldPickupActor(*It))
		{
			HideAndDestroyPickupActor(*It);
		}
	}
}

void AFlashlightPickupItem::SetPickupInteractable(bool bInteractable, bool bForce)
{
	if (!bForce && bPickupInteractable == bInteractable)
	{
		return;
	}

	if (UOblivioGameInstance* GI = Cast<UOblivioGameInstance>(UGameplayStatics::GetGameInstance(this)))
	{
		GI->LoadSessionPersistence();
		if (GI->bFlashlightWorldPickupCollected || GI->bFlashlightAcquired)
		{
			HideAndDestroyWorldPickup();
			return;
		}
	}

	if (!bInteractable)
	{
		ClearOverlappingPlayers();
	}

	bPickupInteractable = bInteractable;

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
