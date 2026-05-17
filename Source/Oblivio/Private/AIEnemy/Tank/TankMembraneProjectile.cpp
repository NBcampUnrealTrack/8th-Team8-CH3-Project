#include "AIEnemy/Tank/TankMembraneProjectile.h"
#include "AIEnemy/Tank/TankPlacentaShellActor.h"
#include "AIEnemy/TankEnemy.h"
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

		// 월드의 모든 다른 양막 투사체와 양방향 IgnoreActorWhenMoving 등록 — 두 양막에서 동시에
		// 6발이 날아도 서로 충돌·정지·자폭하지 않게 한다. emitter 단위가 아니라 월드 단위로 처리해야
		// 양막 A 의 투사체와 양막 B 의 투사체가 교차해도 안전하다.
		for (TActorIterator<ATankMembraneProjectile> It(World); It; ++It)
		{
			ATankMembraneProjectile* const Other = *It;
			if (Other && Other != this)
			{
				IgnoreOtherMembraneProjectile(Other);
			}
		}

		// 탱커 본체·태반 셸(BlockAll…)과 물리 히트하면 비행이 끊기므로 ProjectileMovement 차원에서 무시한다.
		// 플레이어 등 다른 대상에는 그대로 BlockAll 로 반응.
		for (TActorIterator<ATankEnemy> TankIt(World); TankIt; ++TankIt)
		{
			if (ATankEnemy* const Tank = *TankIt; Tank && CollisionSphere)
			{
				CollisionSphere->IgnoreActorWhenMoving(Tank, true);
			}
		}
		for (TActorIterator<ATankPlacentaShellActor> ShellIt(World); ShellIt; ++ShellIt)
		{
			if (ATankPlacentaShellActor* const Shell = *ShellIt; Shell && CollisionSphere)
			{
				CollisionSphere->IgnoreActorWhenMoving(Shell, true);
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

void ATankMembraneProjectile::OnSphereHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!HasAuthority())
	{
		return;
	}

	// 다른 양막 투사체에는 절대 반응하지 않는다(같은 부채꼴/같은 그룹 자가폭발 방지).
	// IgnoreActorWhenMoving 으로 1차 차단하지만 안전망으로 한 번 더 가드.
	if (OtherActor && OtherActor->IsA(ATankMembraneProjectile::StaticClass()))
	{
		return;
	}

	// 자기 자신을 발사한 Tank/Instigator/Owner 와도 충돌 무효(리플레이케이션·자식 편차 대비 이중 검사 제거 불가).
	if (OtherActor && (OtherActor == GetInstigator() || OtherActor == GetOwner()))
	{
		return;
	}

	// 태반 셸·탱커 패스스루( IgnoreActor 가 늦게 깔린 프레임 등 안전망 ).
	if (OtherActor &&
		(OtherActor->IsA(ATankEnemy::StaticClass()) ||
			OtherActor->IsA(ATankPlacentaShellActor::StaticClass())))
	{
		return;
	}

	// 스폰 직후 grace 윈도우 — 양막이 벽/캐릭터 캡슐 가까이서 소환되어 첫 프레임에 hit 이 발생하는
	// 케이스에서 즉시 자폭하지 않도록 잠깐 무시. 시간이 짧아서 정상 비행에는 영향 없음.
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

	if (AOblivioCharacter* const C = Cast<AOblivioCharacter>(OtherActor))
	{
		C->ApplyHealth(Damage);
		UE_LOG(LogTemp, Warning,
			TEXT("[Membrane] %s 플레이어 명중 — Damage=%.1f, Destroy"),
			*GetNameSafe(this), Damage);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Membrane] %s 충돌로 자가 파괴 — Other=%s (%s)"),
			*GetNameSafe(this), *GetNameSafe(OtherActor),
			OtherActor ? *OtherActor->GetClass()->GetName() : TEXT("null"));
	}

	Destroy();
}
