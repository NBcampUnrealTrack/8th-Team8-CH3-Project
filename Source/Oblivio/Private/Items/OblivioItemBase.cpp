#include "Items/OblivioItemBase.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "OblivioCharacter.h"
#include "NiagaraComponent.h"

AOblivioItemBase::AOblivioItemBase()
{
    PrimaryActorTick.bCanEverTick = false;

    // 메쉬 컴포넌트
    ItemMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ItemMesh"));
    RootComponent = ItemMesh;
    ItemMesh->SetCollisionProfileName(TEXT("OverlapAllDynamic"));

    // 오버랩용 스피어 컴포넌트
    InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
    InteractionSphere->SetupAttachment(RootComponent);
    InteractionSphere->SetSphereRadius(150.0f);
    InteractionSphere->SetCollisionProfileName(TEXT("Trigger"));

    LootNiagaraComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("LootNiagaraComponent"));
    LootNiagaraComponent->SetupAttachment(RootComponent);
    LootNiagaraComponent->SetAutoActivate(false);

    InteractText = NSLOCTEXT("OblivioItem", "DefaultInteractText", "Pick Up");
}

void AOblivioItemBase::BeginPlay()
{
    Super::BeginPlay();
    // 델리게이트 바인딩
    InteractionSphere->OnComponentBeginOverlap.AddDynamic(this, &AOblivioItemBase::OnSphereOverlap);
    InteractionSphere->OnComponentEndOverlap.AddDynamic(this, &AOblivioItemBase::OnSphereEndOverlap);

}

void AOblivioItemBase::OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    if (!CanShowNearbyPickupUI() || !OtherActor || OtherActor == this)
    {
        return;
    }

    if (AOblivioCharacter* Player = Cast<AOblivioCharacter>(OtherActor))
    {
        if (ShouldIgnorePlayerPickupOverlap(Player))
        {
            return;
        }

        NotifyPlayerNearbyPickup(Player, true);
    }
}
void AOblivioItemBase::OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    if (AOblivioCharacter* Player = Cast<AOblivioCharacter>(OtherActor))
    {
        NotifyPlayerNearbyPickup(Player, false);
    }
}

bool AOblivioItemBase::ShouldIgnorePlayerPickupOverlap(const AOblivioCharacter* Player)
{
    return IsValid(Player) && Player->ShouldIgnoreItemPickupOverlap();
}

void AOblivioItemBase::NotifyPlayerNearbyPickup(AOblivioCharacter* Player, bool bEnter)
{
    if (!IsValid(Player))
    {
        return;
    }

    if (bEnter)
    {
        if (!CanShowNearbyPickupUI() || ShouldIgnorePlayerPickupOverlap(Player))
        {
            return;
        }

        Player->AddNearbyItem(this);

        if (LootNiagaraComponent)
        {
            LootNiagaraComponent->Activate();
        }
    }
    else
    {
        Player->RemoveNearbyItem(this);

        if (LootNiagaraComponent)
        {
            LootNiagaraComponent->Deactivate();
        }
    }
}

void AOblivioItemBase::SetPickupCollisionEnabled(bool bEnabled)
{
    if (!InteractionSphere)
    {
        return;
    }

    if (!bEnabled)
    {
        TArray<AActor*> OverlappingActors;
        InteractionSphere->GetOverlappingActors(OverlappingActors, AOblivioCharacter::StaticClass());
        for (AActor* Actor : OverlappingActors)
        {
            if (AOblivioCharacter* Player = Cast<AOblivioCharacter>(Actor))
            {
                NotifyPlayerNearbyPickup(Player, false);
            }
        }

        InteractionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        InteractionSphere->SetGenerateOverlapEvents(false);
        return;
    }

    InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionSphere->SetGenerateOverlapEvents(true);
    InteractionSphere->UpdateOverlaps();
}

void AOblivioItemBase::OnInteract_Implementation(AActor* Interactor)
{
    if (AOblivioCharacter* Player = Cast<AOblivioCharacter>(Interactor))
    {
        OnMementoCollected.Broadcast(Player);
    }
}
