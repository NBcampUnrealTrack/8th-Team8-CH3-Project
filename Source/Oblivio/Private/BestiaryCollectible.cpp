#include "BestiaryCollectible.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "NiagaraComponent.h"
#include "OblivioCharacter.h"
#include "OblivioGameInstance.h"
#include "Kismet/GameplayStatics.h"

ABestiaryCollectible::ABestiaryCollectible()
{
    PrimaryActorTick.bCanEverTick = false;

    DocumentMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DocumentMesh"));
    RootComponent = DocumentMesh;

    InteractSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractSphere"));
    InteractSphere->SetupAttachment(RootComponent);
    InteractSphere->SetSphereRadius(150.0f);

    InteractSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

    HighlightEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("HighlightEffect"));
    HighlightEffect->SetupAttachment(RootComponent);
}
void ABestiaryCollectible::BeginPlay()
{
    Super::BeginPlay();

    if (InteractSphere)
    {
        InteractSphere->OnComponentBeginOverlap.AddDynamic(this, &ABestiaryCollectible::OnSphereOverlap);
    }
}

void ABestiaryCollectible::OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    // 겹친 대상이 플레이어 캐릭터인지 확인 후 수집 처리
    AOblivioCharacter* Player = Cast<AOblivioCharacter>(OtherActor);
    if (Player)
    {
        CollectDocument(Player);
    }
}

void ABestiaryCollectible::CollectDocument(AOblivioCharacter* Player)
{
    if (!Player) return;

    UOblivioGameInstance* GI = Cast<UOblivioGameInstance>(UGameplayStatics::GetGameInstance(this));
    if (GI)
    {
        GI->UnlockMonsterEntry(TargetMonsterID);

        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, FString::Printf(TEXT("Bestiary Unlocked: %s"), *TargetMonsterID.ToString()));
    }

    // 파괴되기 직전, 블루프린트에 연결된 델리게이트 이벤트(사운드 재생 등)를 실행
    OnBestiaryCollected.Broadcast(Player);

    Destroy();
}