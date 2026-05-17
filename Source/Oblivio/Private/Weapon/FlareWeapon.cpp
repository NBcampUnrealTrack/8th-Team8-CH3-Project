// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon/FlareWeapon.h"
#include "Weapon/OblivioProjectile.h"

void AFlareWeapon::BeginPlay()
{
	Super::BeginPlay();
	MeshComp->SetVisibility(false);
}

bool AFlareWeapon::UseWeapon()
{
	return Super::UseWeapon();
}

void AFlareWeapon::ExecuteWeaponAttack(FVector TargetLocation)
{
	MeshComp->SetVisibility(false);

	FActorSpawnParameters Params;
	Params.Owner = this;
	AOblivioProjectile* ThrowingProjectile = GetWorld()->SpawnActor<AOblivioProjectile>(
		FlareProjectile,
		GetActorLocation(),
		FRotator::ZeroRotator,
		Params);
	FVector temp = TargetLocation;
	UE_LOG(LogTemp, Warning, TEXT("Throwing %s to %f %f!"), *ThrowingProjectile->GetName(), temp.X, temp.Y);
	if (ThrowingProjectile) ThrowingProjectile->ThrowProjectile(TargetLocation);
}

