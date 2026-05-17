// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon/FlashbangWeapon.h"
#include "Weapon/OblivioProjectile.h"
#include "Components/ArrowComponent.h"

void AFlashbangWeapon::BeginPlay()
{
	Super::BeginPlay();
	MeshComp->SetVisibility(false);
}

//섬광탄 투척 애님 실행
bool AFlashbangWeapon::UseWeapon()
{
	return Super::UseWeapon();
}

//섬광탄 투척중 AnimNotify 받아 섬광탄 스폰
void AFlashbangWeapon::ExecuteWeaponAttack(FVector TargetLocation)
{
	MeshComp->SetVisibility(false);
	UE_LOG(LogTemp, Warning, TEXT("Flashbang weapon ExecuteWeaponAttack"));
	FActorSpawnParameters Params;
	Params.Owner = this;
	AOblivioProjectile* ThrowingProjectile = GetWorld()->SpawnActor<AOblivioProjectile>(
		FlashbangProjectile,
		ArrowComp->GetComponentLocation(),
		FRotator::ZeroRotator,
		Params);
	FVector temp = TargetLocation;
	UE_LOG(LogTemp, Warning, TEXT("Throwing %s to %f %f!"), *ThrowingProjectile->GetName(), temp.X, temp.Y);
	if (IsValid(ThrowingProjectile)) {
		UE_LOG(LogTemp, Warning, TEXT("Setting Projectile launch!"));
		ThrowingProjectile->ThrowProjectile(TargetLocation);
	}
	else UE_LOG(LogTemp, Warning, TEXT("Projectile not valid!"));
}