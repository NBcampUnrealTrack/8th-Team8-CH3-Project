#include "AIEnemy/LuxeaterLaserMeshProbeActor.h"

#include "AIEnemy/LuxeaterEnemy.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ALuxeaterLaserMeshProbeActor::ALuxeaterLaserMeshProbeActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bHitConsumed = false;

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	SetRootComponent(CollisionSphere);
	CollisionSphere->InitSphereRadius(12.f);
	CollisionSphere->SetCollisionProfileName(TEXT("BlockAll"));
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionSphere->SetNotifyRigidBodyCollision(true);

	TranslucentVisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TranslucentVisualMesh"));
	TranslucentVisualMesh->SetupAttachment(CollisionSphere);
	TranslucentVisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TranslucentVisualMesh->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderAsset(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderAsset.Succeeded())
	{
		TranslucentVisualMesh->SetStaticMesh(CylinderAsset.Object);
		TranslucentVisualMesh->SetRelativeScale3D(FVector(0.08f, 0.08f, 0.5f));
		TranslucentVisualMesh->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
	}

	ProjectileMove = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMove"));
	ProjectileMove->UpdatedComponent = CollisionSphere;
	ProjectileMove->MaxSpeed = 2000000.f;
	ProjectileMove->InitialSpeed = 1.f;
	ProjectileMove->bRotationFollowsVelocity = true;
	ProjectileMove->ProjectileGravityScale = 0.f;
	ProjectileMove->bShouldBounce = false;
	ProjectileMove->OnProjectileStop.AddDynamic(this, &ALuxeaterLaserMeshProbeActor::HandleProjectileStopped);
}

void ALuxeaterLaserMeshProbeActor::BeginPlay()
{
	Super::BeginPlay();
}

void ALuxeaterLaserMeshProbeActor::ArmProbe(ALuxeaterEnemy* const OwningBoss,
	FVector const& StartWorld, FVector const& AimPointWorld, float const ProjectileSpeedUU,
	TObjectPtr<UNiagaraSystem> const ImpactFx, UNiagaraSystem* const BossFallbackImpactFx,
	float const SphereRadiusUU, float const LifetimePadSeconds)
{
	CachedBoss = OwningBoss;
	OriginCachedWorld = StartWorld;
	AimPointCached = AimPointWorld;
	ImpactFxSystem = ImpactFx;
	FallbackFxSystemFromBoss = BossFallbackImpactFx;
	FallbackLifePadSeconds = FMath::Max(LifetimePadSeconds, 0.01f);

	if (CollisionSphere)
	{
		CollisionSphere->SetSphereRadius(FMath::Max(SphereRadiusUU, 1.f));
		if (OwningBoss)
		{
			CollisionSphere->IgnoreActorWhenMoving(OwningBoss, true);
		}
	}

	SetActorLocation(StartWorld);
	bHitConsumed = false;

	FVector const Dir = (AimPointWorld - StartWorld).GetSafeNormal();
	if (Dir.IsNearlyZero())
	{
		SetLifeSpan(0.01f);
		return;
	}

	if (ProjectileMove)
	{
		ProjectileMove->Velocity = Dir * ProjectileSpeedUU;
		ProjectileMove->UpdateComponentVelocity();
	}

	const float Dist = FVector::Dist(StartWorld, AimPointWorld);
	const float TravelTime =
		ProjectileSpeedUU > KINDA_SMALL_NUMBER ? (Dist / ProjectileSpeedUU) + FallbackLifePadSeconds
											   : FallbackLifePadSeconds;

	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().SetTimer(
			FailsafeTimerHandle, this, &ALuxeaterLaserMeshProbeActor::OnFailsafeTimeout, TravelTime,
			false);
	}
}

void ALuxeaterLaserMeshProbeActor::HandleProjectileStopped(FHitResult const& ImpactResult)
{
	if (bHitConsumed)
	{
		return;
	}
	if (!ImpactResult.IsValidBlockingHit())
	{
		return;
	}
	ConsumeHit(ImpactResult);
}

void ALuxeaterLaserMeshProbeActor::ConsumeHit(FHitResult const& Hit)
{
	if (bHitConsumed)
	{
		return;
	}
	bHitConsumed = true;

	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().ClearTimer(FailsafeTimerHandle);
	}

	if (ProjectileMove && ProjectileMove->UpdatedComponent)
	{
		ProjectileMove->StopMovementImmediately();
		ProjectileMove->Velocity = FVector::ZeroVector;
	}
	if (CollisionSphere)
	{
		CollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (TranslucentVisualMesh)
	{
		TranslucentVisualMesh->SetHiddenInGame(true);
	}

	if (UWorld* W = GetWorld())
	{
		UNiagaraSystem* ImpactToPlay = ImpactFxSystem;
		if (!IsValid(ImpactToPlay))
		{
			ImpactToPlay = FallbackFxSystemFromBoss;
		}

		FVector Normal = Hit.ImpactNormal.GetSafeNormal();
		if (Normal.IsNearlyZero())
		{
			Normal = (Hit.TraceEnd - Hit.TraceStart).GetSafeNormal();
			if (Normal.IsNearlyZero())
			{
				Normal = FVector::UpVector;
			}
		}
		FRotator const BurstRot = UKismetMathLibrary::MakeRotFromZ(Normal);

		if (IsValid(ImpactToPlay))
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				W, ImpactToPlay, Hit.ImpactPoint, BurstRot, FVector::OneVector, true);
		}
	}

	if (ALuxeaterEnemy* Boss = CachedBoss.Get())
	{
		Boss->LaserResolveProjectileImpact(Hit);
	}

	SetLifeSpan(0.02f);
}

void ALuxeaterLaserMeshProbeActor::OnFailsafeTimeout()
{
	if (bHitConsumed)
	{
		return;
	}
	bHitConsumed = true;

	if (ALuxeaterEnemy* Boss = CachedBoss.Get())
	{
		Boss->LaserRunTraceFallback(OriginCachedWorld);
	}

	SetLifeSpan(0.01f);
}
