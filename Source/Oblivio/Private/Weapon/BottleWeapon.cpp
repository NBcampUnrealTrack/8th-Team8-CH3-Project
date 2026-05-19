// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon/BottleWeapon.h"
#include "Weapon/OblivioProjectile.h"
#include "Components/ArrowComponent.h"


void ABottleWeapon::BeginPlay()
{
	Super::BeginPlay();
	MeshComp->SetVisibility(false);
}

bool ABottleWeapon::UseWeapon()
{
	return Super::UseWeapon();
}

void ABottleWeapon::ExecuteWeaponAttack(FVector TargetLocation)
{
	MeshComp->SetVisibility(false);
	UE_LOG(LogTemp, Warning, TEXT("Flashbang weapon ExecuteWeaponAttack"));
	FActorSpawnParameters Params;
	Params.Owner = this;
	AOblivioProjectile* ThrowingProjectile = GetWorld()->SpawnActor<AOblivioProjectile>(
		BottleProjectile,
		ArrowComp->GetComponentLocation(),
		FRotator::ZeroRotator,
		Params);
	FVector temp = TargetLocation;
	if (IsValid(ThrowingProjectile)) {
		UE_LOG(LogTemp, Warning, TEXT("Throwing %s to %f %f!"), *ThrowingProjectile->GetName(), temp.X, temp.Y);
		ThrowingProjectile->ThrowProjectile(TargetLocation);
	}
	else UE_LOG(LogTemp, Warning, TEXT("Projectile not valid!"));
}

