// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon/OblivioProjectile.h"
#include "Components/AudioComponent.h"
#include "Components/SphereComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "OblivioComponents/SoundPropagationComponent.h"

// Sets default values
AOblivioProjectile::AOblivioProjectile()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	SphereComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
	RootComponent = SphereComp;
    SphereComp->SetCollisionProfileName("BlockAllDynamic");
    SphereComp->SetSimulatePhysics(true);

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetupAttachment(RootComponent);
    MeshComp->SetCollisionProfileName("NoCollision");

	ProjectileComp = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileComp"));
	ProjectileComp->SetUpdatedComponent(RootComponent);
    //튕김 현상 적용
    ProjectileComp->bShouldBounce = true;
    ProjectileComp->Bounciness = 0.6f;
    ProjectileComp->Friction = 0.2f;
    ProjectileComp->BounceVelocityStopSimulatingThreshold = 2000.f;

	TriggerSoundComp = CreateDefaultSubobject<UAudioComponent>(TEXT("TriggerSoundComp"));
	TriggerSoundComp->SetupAttachment(RootComponent);
    TriggerSoundComp->bAutoActivate = false;

	ReflectionSoundComp = CreateDefaultSubobject<UAudioComponent>(TEXT("ReflectionAudioComp"));
	ReflectionSoundComp->SetupAttachment(RootComponent);
    ReflectionSoundComp->bAutoActivate = false;

    SoundPropagationComp = CreateDefaultSubobject<USoundPropagationComponent>(TEXT("SoundPropagationComp"));

    PrimaryActorTick.bCanEverTick = true;
    SpeedPerDistance = 0.001f;
}

// Called when the game starts or when spawned
void AOblivioProjectile::BeginPlay()
{
    Super::BeginPlay();
    SphereComp->OnComponentHit.AddDynamic(this, &AOblivioProjectile::PlayReflectionSound);
    GetWorld()->GetTimerManager().SetTimer(LifeTimerHandle, this, &AOblivioProjectile::DestroyProjectile, MaxLifespan);

    //충돌 회전 초기화
    SpinVelocity = FVector::ZeroVector;
}

//목표지점까지의 초기 속도 설정
void AOblivioProjectile::ThrowProjectile(FVector TargetLocation)
{
    FVector StartLocation = GetActorLocation();

    FVector NewVelocity;
    bool bSuccess = UGameplayStatics::SuggestProjectileVelocity(
        this,
        NewVelocity,
        StartLocation,
        TargetLocation,
        SpeedPerDistance * (StartLocation - TargetLocation).Length(),
        false,
        0.0f,
        0.0f,
        ESuggestProjVelocityTraceOption::DoNotTrace);

    if (!bSuccess) {
        UE_LOG(LogTemp, Warning, TEXT("velocity calculation failed!: %f %f %f"), NewVelocity.X, NewVelocity.Y, NewVelocity.Z);
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("setting the projectile velocity to %f %f %f"), NewVelocity.X, NewVelocity.Y, NewVelocity.Z);
    ProjectileComp->Velocity = NewVelocity;
}



//충돌시 충격 SFX 출력, 주변 에너미에게 소리 자극 전파
void AOblivioProjectile::PlayReflectionSound(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
    UE_LOG(LogTemp, Warning, TEXT("Hit with %s!"), *OtherActor->GetName());
    if (IsValid(SoundPropagationComp))
    {
        UE_LOG(LogTemp, Warning, TEXT("Sound Propagates!"));
        SoundPropagationComp->PropagateSound();
    }
    if (IsValid(ReflectionSoundComp) && IsValid(ReflectionSoundComp->Sound)) {
        UE_LOG(LogTemp, Warning, TEXT("PlaingReflectionSound!"));
        ReflectionSoundComp->Play();
    }

    SpinVelocity += FVector(
        FMath::RandRange(-ImpactRotationRange, ImpactRotationRange),
        FMath::RandRange(-ImpactRotationRange, ImpactRotationRange),
        FMath::RandRange(-ImpactRotationRange, ImpactRotationRange)
    );
}

//투사체 발동시 사운드 (폭팔, 점화등)
void AOblivioProjectile::PlayTriggerSound()
{

}

//수명 지나고 투사체 삭제
void AOblivioProjectile::DestroyProjectile()
{
    Destroy();
}

// Called every frame
void AOblivioProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
    
    MeshComp->AddLocalRotation(FRotator(SpinVelocity.X * DeltaTime,SpinVelocity.Y * DeltaTime,SpinVelocity.Z * DeltaTime));

    SpinVelocity = FMath::VInterpTo(
        SpinVelocity,
        FVector::ZeroVector,
        DeltaTime,
        2.f
    );
}

