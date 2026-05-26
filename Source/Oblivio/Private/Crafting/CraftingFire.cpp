#include "Crafting/CraftingFire.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"

#include "OblivioCharacter.h"
#include "AIEnemy/EnemyBase.h"
#include "OblivioComponents/CombatInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Combat/LightDamageType.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"


ACraftingFire::ACraftingFire()
{
    PrimaryActorTick.bCanEverTick = true;

    FireLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FireLight"));
    FireLight->SetupAttachment(RootComponent);
    FireLight->SetIntensity(0.0f); // 설치 전엔 꺼둠

    FireParticleComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("FireParticleComponent"));
    FireParticleComponent->SetupAttachment(RootComponent);
    FireParticleComponent->bAutoActivate = false;

    EffectRange = CreateDefaultSubobject<USphereComponent>(TEXT("EffectRange"));
    EffectRange->SetupAttachment(RootComponent);
    EffectRange->SetSphereRadius(4000.0f);

    FireLight->SetAttenuationRadius(400.0f);

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
    UE_LOG(LogTemp, Warning, TEXT("OnPlace Called"));
    bIsActive = true;
    RemainingTime = BurnDuration;
    FireLight->SetIntensity(LightIntensity);
    BaseLightIntensity = LightIntensity;

    if (IsValid(FireParticleComponent) && IsValid(FireParticleComponent->GetFXSystemAsset()))
    {
        FireParticleComponent->Activate(true);
    }
}

void ACraftingFire::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bIsActive)
    {
        RemainingTime -= DeltaTime;
        ApplyFireEffect(DeltaTime);

        float FlickerIntensity = BaseLightIntensity + FMath::RandRange(-500.0f, 500.0f);
        FireLight->SetIntensity(FlickerIntensity);

        if (RemainingTime <= 0.0f)
        {
            bIsActive = false;
            FireLight->SetIntensity(0.0f); // 불이 꺼짐
            FireParticleComponent->Deactivate();
            Destroy();
        }
    }
}

void ACraftingFire::ApplyFireEffect(float DeltaTime)
{
    TArray<AActor*> OverlappingActors;
    EffectRange->GetOverlappingActors(OverlappingActors);

    for (AActor* Actor : OverlappingActors)
    {
        if (!IsValid(Actor) || Actor == this) continue;
        if (AOblivioCharacter* Player = Cast<AOblivioCharacter>(Actor))
        {
            if (Player->CurrentHealth > 0 && Player->CurrentHealth < Player->MaxHealth)
            {
                Player->CurrentHealth = FMath::Clamp(Player->CurrentHealth + (3.0f * DeltaTime), 0.0f, Player->MaxHealth);
                Player->OnPlayerDamaged.Broadcast(0.0f, Player->CurrentHealth, Player->MaxHealth);
            }

            // 2. 배터리가 깎여있을 때만 회복
            if (Player->Battery < 100.0f)
            {
                Player->Battery = FMath::Clamp(Player->Battery + (5.0f * DeltaTime), 0.0f, 100.0f);
                Player->OnBatteryChanged.Broadcast(Player->Battery, 100.0f);
            }

            if (!Player->bIsFlashlightOn)
            {
                Player->bIsFlashlightOn = true;
                Player->UpdateFlashlightVisuals();
            }
        }
        else if (AEnemyBase* Enemy = Cast<AEnemyBase>(Actor))
        {
            float FireDamage = 10.0f * DeltaTime;
            UGameplayStatics::ApplyDamage(Enemy, FireDamage, nullptr, this, ULightDamageType::StaticClass());
            Enemy->ApplyCCStun(3.0f); // 스턴 적용
        }

    }
}
