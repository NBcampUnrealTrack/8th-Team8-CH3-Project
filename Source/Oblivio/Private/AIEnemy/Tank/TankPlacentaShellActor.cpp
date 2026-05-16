#include "AIEnemy/Tank/TankPlacentaShellActor.h"
#include "AIEnemy/TankEnemy.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "UObject/ConstructorHelpers.h"

static FName const TankPlacentaShellAttachSocket = NAME_None;

ATankPlacentaShellActor::ATankPlacentaShellActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("TankPlacentaCollisionSphere"));
	SetRootComponent(CollisionSphere);
	CollisionSphere->SetSphereRadius(FMath::Max(10.f, ShellSphereRadiusCm));
	CollisionSphere->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	CollisionSphere->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	CollisionSphere->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	CollisionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	CollisionSphere->SetGenerateOverlapEvents(false);
	CollisionSphere->CanCharacterStepUpOn = ECB_No;

	VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TankPlacentaVisualMesh"));
	VisualMesh->SetupAttachment(CollisionSphere);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualMesh->SetCastShadow(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereAsset(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereAsset.Succeeded())
	{
		VisualMesh->SetStaticMesh(SphereAsset.Object);
		const float R = FMath::Max(10.f, ShellSphereRadiusCm);
		// 엔진 Sphere 기본 반경 50UU — 스케일로 ShellSphereRadiusCm 근사
		VisualMesh->SetRelativeScale3D(FVector(R / 50.f));
	}
}

void ATankPlacentaShellActor::BindToTank_Server(ATankEnemy* const OwnerTank, float const InRadiusCm, float const InMaxHealth)
{
	if (!HasAuthority() || !IsValid(OwnerTank))
	{
		return;
	}
	TankOwner = OwnerTank;

	ShellSphereRadiusCm = FMath::Max(10.f, InRadiusCm);
	ShellMaxHealth = FMath::Max(1.f, InMaxHealth);
	ShellCurrentHealth = ShellMaxHealth;

	if (CollisionSphere)
	{
		CollisionSphere->SetSphereRadius(ShellSphereRadiusCm);
	}
	if (VisualMesh && VisualMesh->GetStaticMesh())
	{
		const float R = ShellSphereRadiusCm;
		VisualMesh->SetRelativeScale3D(FVector(R / 50.f));
	}

	AttachToActor(OwnerTank, FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		TankPlacentaShellAttachSocket);
	SetOwner(OwnerTank);

	// 캡슐 중심에 맞춤(부착 오프셋 0 — 루트가 탱커 캡슐 중심)
	if (USceneComponent* const TankRoot = OwnerTank->GetRootComponent())
	{
		SetActorRelativeLocation(FVector::ZeroVector);
		SetActorRelativeRotation(FRotator::ZeroRotator);
	}
}

float ATankPlacentaShellActor::GetTankPlacentaShellHealthPercent() const
{
	if (ShellMaxHealth <= KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}
	return FMath::Clamp(ShellCurrentHealth / ShellMaxHealth, 0.f, 1.f);
}

float ATankPlacentaShellActor::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	if (!HasAuthority())
	{
		return 0.f;
	}
	if (!IsTankPlacentaShellAlive() || DamageAmount <= KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}

	ShellCurrentHealth = FMath::Max(0.f, ShellCurrentHealth - DamageAmount);
	OnTankPlacentaShellDamaged(DamageAmount, ShellCurrentHealth);

	if (ShellCurrentHealth <= KINDA_SMALL_NUMBER)
	{
		BreakShell_Server(EventInstigator, DamageCauser);
	}
	return DamageAmount;
}

void ATankPlacentaShellActor::BreakShell_Server(AController* EventInstigator, AActor* DamageCauser)
{
	(void)EventInstigator;
	(void)DamageCauser;
	OnTankPlacentaShellBroken();
	if (ATankEnemy* const T = TankOwner.Get())
	{
		T->NotifyTankPlacentaShellBroken_Server(this);
	}
	Destroy();
}

void ATankPlacentaShellActor::UnbindAndDestroy_Server()
{
	if (!HasAuthority())
	{
		return;
	}
	OnTankPlacentaShellBroken();
	Destroy();
}

void ATankPlacentaShellActor::OnTankPlacentaShellDamaged_Implementation(float DamageAmount, float ShellHpAfter)
{
	(void)DamageAmount;
	(void)ShellHpAfter;
}

void ATankPlacentaShellActor::OnTankPlacentaShellBroken_Implementation()
{
}
