#include "Crafting/ObstacleBase.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"

AObstacleBase::AObstacleBase()
{
    PrimaryActorTick.bCanEverTick = false;

    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
    RootComponent = MeshComponent;

    // 기본적으로 몬스터 이동에 영향을 주도록 설정
    MeshComponent->SetCanEverAffectNavigation(true);

    CurrentHealth = MaxHealth;

}

void AObstacleBase::BeginPlay()
{
	Super::BeginPlay();
    OriginalMaterial = MeshComponent->GetMaterial(0);
}

void AObstacleBase::SetGhostMode(bool bIsGhost, bool bCanPlace)
{
    // 현재 상태를 업데이트
    CurrentState = bIsGhost ? EObstacleState::Ghost : EObstacleState::Placed;

    if (bIsGhost)
    {
        if (GhostMaterial && MeshComponent)
        {
            UMaterialInstanceDynamic* DynamicGhostMat = MeshComponent->CreateDynamicMaterialInstance(0, GhostMaterial);

            if (DynamicGhostMat)
            {
                // FLinearColor(R, G, B, A)
                FLinearColor TargetColor = bCanPlace ?
                    FLinearColor(1.0f, 1.0f, 1.0f, 0.5f) : // 설치 가능/치트 켜짐: 초록색
                    FLinearColor(1.0f, 0.0f, 0.0f, 0.5f);  // 자원 부족/거리 초과: 빨간색

                DynamicGhostMat->SetVectorParameterValue(FName("TintColor"), TargetColor);
            }
        }

        if (MeshComponent)
        {
            MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }
    }
    else
    {
        // 실제 설치 완료 상태
        if (OriginalMaterial && MeshComponent)
        {
            MeshComponent->SetMaterial(0, OriginalMaterial);
            MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        }
    }
}

void AObstacleBase::OnPlaced()
{
    CurrentState = EObstacleState::Placed;
    MeshComponent->SetMaterial(0, OriginalMaterial);
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

    // 설치 완료 후 네비게이션 리빌드 유도
    MeshComponent->SetCanEverAffectNavigation(true);

    bIsPlaced = true;
}

float AObstacleBase::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    if (CurrentState != EObstacleState::Placed) return 0.0f;

    CurrentHealth -= DamageAmount;

    if (CurrentHealth <= 0.0f)
    {
        CurrentState = EObstacleState::Destroyed;
        Destroy();
    }

    return DamageAmount;
}

void AObstacleBase::HandleDestruction()
{
    if (MeshComponent)
    {
        MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        MeshComponent->SetVisibility(false);
    }

    if (IsValid(DestructionSound))
    {
        UGameplayStatics::PlaySoundAtLocation(this, DestructionSound, GetActorLocation());
    }
    if (IsValid(DestructionEffect))
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), DestructionEffect, GetActorLocation(), GetActorRotation());

    }
    FTimerHandle DestroyTimeHandler;
    FTimerDelegate TimerDelegate;
    TWeakObjectPtr<AObstacleBase> WeakThis(this);
    TimerDelegate.BindLambda([WeakThis]()
        {
            if (WeakThis.IsValid())
            {
                WeakThis->Destroy();
            }
        });
    GetWorldTimerManager().SetTimer(DestroyTimeHandler, TimerDelegate, 2.0f, false);
}

