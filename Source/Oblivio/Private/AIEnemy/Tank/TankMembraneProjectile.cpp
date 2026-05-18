#include "AIEnemy/Tank/TankMembraneProjectile.h"

#include "OblivioCharacter.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "UObject/ConstructorHelpers.h"

ATankMembraneProjectile::ATankMembraneProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	SetRootComponent(CollisionSphere);
	CollisionSphere->InitSphereRadius(12.f);
	CollisionSphere->SetCollisionProfileName(TEXT("BlockAll"));
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionSphere->SetNotifyRigidBodyCollision(true);
	CollisionSphere->OnComponentHit.AddDynamic(this, &ATankMembraneProjectile::OnSphereHit);

	VisualSphere = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualSphere"));
	VisualSphere->SetupAttachment(CollisionSphere);
	VisualSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualSphere->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereAsset(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereAsset.Succeeded())
	{
		VisualSphere->SetStaticMesh(SphereAsset.Object);
		VisualSphere->SetRelativeScale3D(FVector(0.075f));
	}

	ProjectileMove = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMove"));
	ProjectileMove->UpdatedComponent = CollisionSphere;
	ProjectileMove->bRotationFollowsVelocity = true;
	ProjectileMove->ProjectileGravityScale = 0.f;
	ProjectileMove->bShouldBounce = false;
}

void ATankMembraneProjectile::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* const World = GetWorld())
	{
		SpawnWorldSeconds = World->GetTimeSeconds();

		// 월드의 모든 다른 양막 투사체와 양방향 IgnoreActorWhenMoving 등록
		for (TActorIterator<ATankMembraneProjectile> It(World); It; ++It)
		{
			ATankMembraneProjectile* const Other = *It;
			if (Other && Other != this)
			{
				IgnoreOtherMembraneProjectile(Other);
			}
		}
	}
}

void ATankMembraneProjectile::InitializeFlight(float InDamage, float InSpeedUU, FVector const& DirectionWorld)
{
	Damage = InDamage;
	bFlightInitialized = true;

	FVector Dir = DirectionWorld.GetSafeNormal();
	if (Dir.IsNearlyZero())
	{
		Dir = FVector::ForwardVector;
	}

	if (TrailNiagaraSystem)
	{
		UNiagaraFunctionLibrary::SpawnSystemAttached(TrailNiagaraSystem,
			CollisionSphere, NAME_None, FVector::ZeroVector, Dir.Rotation(), EAttachLocation::KeepRelativeOffset,
			true, true);
	}

	if (ProjectileMove)
	{
		ProjectileMove->Velocity = Dir * InSpeedUU;
		ProjectileMove->UpdateComponentVelocity();
	}

	const float MaxFlight = 32000.f;
	SetLifeSpan(FMath::Clamp(MaxFlight / FMath::Max(InSpeedUU, 1.f), 0.5f, 12.f));
}

void ATankMembraneProjectile::IgnoreOtherMembraneProjectile(ATankMembraneProjectile* Other)
{
	if (!Other || Other == this)
	{
		return;
	}
	if (CollisionSphere)
	{
		CollisionSphere->IgnoreActorWhenMoving(Other, true);
	}
	if (Other->CollisionSphere)
	{
		Other->CollisionSphere->IgnoreActorWhenMoving(this, true);
	}
}

void ATankMembraneProjectile::ProceedThroughPassthroughHit(AActor* OtherActor)
{
	if (!ProjectileMove || !CollisionSphere || !IsValid(OtherActor))
	{
		return;
	}

	CollisionSphere->IgnoreActorWhenMoving(OtherActor, true);

	FVector const SavedVel = ProjectileMove->Velocity;
	ProjectileMove->Velocity = SavedVel;
	ProjectileMove->UpdateComponentVelocity();
}

void ATankMembraneProjectile::OnSphereHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!HasAuthority())
	{
		return;
	}

	// 다른 양막 투사체
	if (OtherActor && OtherActor->IsA(ATankMembraneProjectile::StaticClass()))
	{
		return;
	}

	// 발사 원(Tank)·Owner
	if (OtherActor && (OtherActor == GetInstigator() || OtherActor == GetOwner()))
	{
		return;
	}

	// 스폰 직후 grace 윈도우
	if (UWorld* const World = GetWorld())
	{
		const double Now = World->GetTimeSeconds();
		if (SpawnHitGraceSeconds > 0.0 && (Now - SpawnWorldSeconds) < static_cast<double>(SpawnHitGraceSeconds))
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[Membrane] %s 스폰 grace(%.2fs) 안에서 hit 무시 — Other=%s Loc=%s"),
				*GetNameSafe(this), SpawnHitGraceSeconds,
				*GetNameSafe(OtherActor), *GetActorLocation().ToString());
			return;
		}
	}

	// 플레이어: 데미지 후 투사체 제거.
	if (AOblivioCharacter* const Player = Cast<AOblivioCharacter>(OtherActor))
	{
		Player->ApplyHealth(Damage);
		Destroy();
		return;
	}

	// 플레이어 제외: 충돌만 무시하고 비행 유지.
	if (IsValid(OtherActor))
	{
		ProceedThroughPassthroughHit(OtherActor);
	}
}
