#include "Crafting/CraftingFire.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"

#include "OblivioCharacter.h"
#include "AIEnemy/EnemyBase.h"
#include "OblivioComponents/CombatInterface.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

ACraftingFire::ACraftingFire()
{
    PrimaryActorTick.bCanEverTick = true;

    FireLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FireLight"));
    FireLight->SetupAttachment(RootComponent);
    FireLight->SetIntensity(0.0f); // 설치 전엔 꺼둠

    EffectRange = CreateDefaultSubobject<USphereComponent>(TEXT("EffectRange"));
    EffectRange->SetupAttachment(RootComponent);
    EffectRange->SetSphereRadius(400.0f);

    // 기본 비용 설정
    WoodCost = 2;
    BatteryCostPercent = 5.0f;
    bIsActive = false;
}

void ACraftingFire::BeginPlay()
{
    Super::BeginPlay();
}

void ACraftingFire::OnPlaced()
{
    Super::OnPlaced();

    bIsActive = true;
    RemainingTime = BurnDuration;
    FireLight->SetIntensity(LightIntensity);
}

void ACraftingFire::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bIsActive)
    {
        RemainingTime -= DeltaTime;
        ApplyFireEffect();

        if (RemainingTime <= 0.0f)
        {
            bIsActive = false;
            FireLight->SetIntensity(0.0f); // 불이 꺼짐
            Destroy();
        }
    }
}

void ACraftingFire::ApplyFireEffect()
{
    TArray<AActor*> OverlappingActors;
    EffectRange->GetOverlappingActors(OverlappingActors);

    for (AActor* Actor : OverlappingActors)
    {
        if (!IsValid(Actor) || Actor == this) continue;
        if (AOblivioCharacter* Player = Cast<AOblivioCharacter>(Actor))
        {
            Player->CurrentHealth = Player->MaxHealth;
            Player->Battery = 100.0f;
            Player->OnPlayerDamaged.Broadcast(0.0f, Player->CurrentHealth, Player->MaxHealth);
            if (!Player->bIsFlashlightOn)
            {
                Player->bIsFlashlightOn = true;
                Player->UpdateFlashlightVisuals();
            }
            continue;
        }
        // 몬스터 클래스인지 확인 후 스턴 혹은 속도 저하 적용
        else if (AEnemyBase* Enemy = Cast<AEnemyBase>(Actor))
        {
            Enemy->ApplyCCSlow(0.5f, 1.0f);
        }
    }
}
