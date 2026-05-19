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
    if (OtherActor && OtherActor != this)
    {
        AOblivioCharacter* Player = Cast<AOblivioCharacter>(OtherActor);
        if (Player)
        {
            Player->AddNearbyItem(this);


            if (LootNiagaraComponent)
            {
                LootNiagaraComponent->Activate();
            }
        }
    }
}
void AOblivioItemBase::OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    if (AOblivioCharacter* Player = Cast<AOblivioCharacter>(OtherActor))
    {
        // 캐릭터에게서 아이템 정보 제거 (UI 끄기 신호)
        Player->RemoveNearbyItem(this);
        //ItemMesh->SetRenderCustomDepth(false);
        if (LootNiagaraComponent)
        {
            LootNiagaraComponent->Deactivate();
        }
    }
}

void AOblivioItemBase::OnInteract_Implementation(AActor* Interactor)
{
    if (AOblivioCharacter* Player = Cast<AOblivioCharacter>(Interactor))
    {
        OnMementoCollected.Broadcast(Player);
    }
}
