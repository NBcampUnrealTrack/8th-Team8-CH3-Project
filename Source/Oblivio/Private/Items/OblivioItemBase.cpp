#include "Items/OblivioItemBase.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "OblivioCharacter.h"

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
            //나중에 pickup UI랑 연동
            Player->SetNearbyItem(this);
            ItemMesh->SetRenderCustomDepth(true);
            ItemMesh->SetCustomDepthStencilValue(255);
            // 나중에 만들 InventoryComponent의 AddItem 함수를 여기서 호출
            // if (Player->InventoryComponent->AddItem(this)) 
            // {
            //     Destroy();
            // }

            // 현재는 로그로 확인
            UE_LOG(LogTemp, Warning, TEXT("Overlap with Player! Item: %s"), *ItemName.ToString());
        }
    }
}
void AOblivioItemBase::OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    if (AOblivioCharacter* Player = Cast<AOblivioCharacter>(OtherActor))
    {
        // 캐릭터에게서 아이템 정보 제거 (UI 끄기 신호)
        Player->SetNearbyItem(nullptr);
        ItemMesh->SetRenderCustomDepth(false);
        UE_LOG(LogTemp, Warning, TEXT("Overlap Ended with Player! Item: %s"), *ItemName.ToString());
    }
}

void AOblivioItemBase::OnInteract_Implementation(AActor* Interactor)
{
}
