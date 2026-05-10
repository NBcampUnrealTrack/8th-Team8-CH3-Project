#include "Crafting/CraftingSoundDecoy.h"
#include "Components/StaticMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Components/SphereComponent.h"
#include "TimerManager.h"
#include "GameFramework/Character.h" 
#include "Kismet/GameplayStatics.h"

ACraftingSoundDecoy::ACraftingSoundDecoy()
{
    // 미끼는 체력이 낮게 설정
    MaxHealth = 50.0f;
    CurrentHealth = MaxHealth;

    // 설치 비용 설정
    WoodCost = 1;
    IronCost = 2;

    NoiseInterval = 3.0f; // 3초마다 소음 발생
    NoiseRange = 1500.0f; // 1500 반경 내의 몬스터 어그로 끌기

    // 오디오 컴포넌트 생성 및 루트에 부착
    AudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("AudioComponent"));
    AudioComponent->SetupAttachment(RootComponent);
    AudioComponent->bAutoActivate = false; // 설치 전에는 소리가 나지 않도록 꺼둠

    TriggerArea = CreateDefaultSubobject<USphereComponent>(TEXT("TriggerArea"));
    TriggerArea->SetupAttachment(RootComponent);
    TriggerArea->SetSphereRadius(150.0f); // 밟는 판정 크기
    TriggerArea->SetCollisionEnabled(ECollisionEnabled::NoCollision); // 설치 전엔 작동 안 함
}

void ACraftingSoundDecoy::BeginPlay()
{
    Super::BeginPlay();

    if (TriggerArea)
    {
        TriggerArea->OnComponentBeginOverlap.AddDynamic(this, &ACraftingSoundDecoy::OnOverlapBegin);
    }
}

void ACraftingSoundDecoy::OnPlaced()
{
    Super::OnPlaced();

    // 설치가 완료되면 눈에 보이는/들리는 오디오 재생 시작
    if (AudioComponent)
    {
        AudioComponent->Play();
    }

    // 타이머를 시작하여 주기적으로 몬스터용 소음(Noise) 이벤트 발생
    GetWorldTimerManager().SetTimer(NoiseTimerHandle, this, &ACraftingSoundDecoy::EmitNoise, NoiseInterval, true);

    if (TriggerArea)
    {
        TriggerArea->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        TriggerArea->SetCollisionResponseToAllChannels(ECR_Ignore);
        TriggerArea->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    }

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan, TEXT("Sound Decoy Placed! Trap Armed!"));
    }
}

void ACraftingSoundDecoy::EmitNoise()
{
    // 언리얼 엔진의 기본 AI 청각 시스템이 감지할 수 있는 노이즈 발생
    MakeNoise(1.0f, GetInstigator(), GetActorLocation(), NoiseRange);
}

void ACraftingSoundDecoy::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (OtherActor && OtherActor != this && OtherActor->IsA(ACharacter::StaticClass()))
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("Trap Stepped On! LOUD NOISE!"));
        }

        if (TrapTriggerSound)
        {
            UGameplayStatics::PlaySoundAtLocation(this, TrapTriggerSound, GetActorLocation());
        }
        MakeNoise(1.0f, GetInstigator(), GetActorLocation(), NoiseRange * 1.5f);
    }
}