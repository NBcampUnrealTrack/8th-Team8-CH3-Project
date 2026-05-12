//WeaponBase.cpp
#include "Weapon/WeaponBase.h"
#include "Components/SphereComponent.h"
#include "OblivioComponents/LightAttackComponent.h"

// Sets default values
AWeaponBase::AWeaponBase()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	SphereComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
	RootComponent = SphereComp;
	SphereComp->SetSimulatePhysics(false);
	SphereComp->SetCollisionProfileName("BlockAllDynamic");

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetupAttachment(RootComponent);
	MeshComp->SetCollisionProfileName("NoCollision");
	MeshComp->SetSimulatePhysics(false);

	LightAttackComp = CreateDefaultSubobject<ULightAttackComponent>(TEXT("LightAttackComp"));
	LightAttackComp->SetupAttachment(RootComponent);
}

// Called when the game starts or when spawned
void AWeaponBase::BeginPlay()
{
	Super::BeginPlay();
	UseWeapon();
}

// Called every frame
void AWeaponBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AWeaponBase::UseWeapon()
{
	//LightAttackComp->CreateLightAttack(GetActorLocation(), GetActorForwardVector());
}

void AWeaponBase::StopWeapon()
{
	//LightAttackComp->TurnOffLight();
}

void AWeaponBase::ChangeWeaponAngle(float DeltaAngle)
{
	//UE_LOG(LogTemp, Warning, TEXT("ChangeWeaponAngle Called"));
	//LightAttackComp->ChangeLightAngle(DeltaAngle);
}
