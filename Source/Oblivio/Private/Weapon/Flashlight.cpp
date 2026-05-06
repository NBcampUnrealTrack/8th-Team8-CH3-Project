// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon/Flashlight.h"
#include "OblivioComponents/LightAttackComponent.h"

AFlashlight::AFlashlight()
{
	AttackInterval = 0.2f;
	LightAttackComp->bIsConcentrated = true;
	LightAttackComp->LightIntensityScale = 10000.f;
	LightAttackComp->LightTime = 5.f;
	LightAttackComp->MinAngle = 10.f;
	LightAttackComp->MaxAngle = 160.f;
	LightAttackComp->DistancePerAngle = 3.f;
	LightAttackComp->DamagePerAngle = 0.02f;
	LightAttackComp->Damage = 3.f;
	LightAttackComp->LightAngle = 60.f;
	LightAttackComp->LightDistance = 1000.f;
	LightAttackComp->MaxDamageDistance = 500.f;
	LightAttackComp->DamageAttenuationRate = 2.f;
	LightAttackComp->BasicLightColor = FColor::White;
}

void AFlashlight::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogTemp, Warning, TEXT("BeginPlay"));
}

void AFlashlight::UseWeapon()
{
	if (!IsValid(LightAttackComp)) return;
	if (!GetWorld()->GetTimerManager().IsTimerActive(AttackTimerHandle)) {
		UE_LOG(LogTemp, Warning, TEXT("Setting Timer"));

		GetWorld()->GetTimerManager().SetTimer(
			AttackTimerHandle,
			[this]() {
				FVector SourceLocation = LightAttackComp->GetComponentLocation();
				FVector LightDirection = LightAttackComp->GetForwardVector();
				LightAttackComp->CreateLightAttack(SourceLocation, LightDirection); },
			AttackInterval,
			true);
		LightAttackComp->CreateLightAttack(GetActorLocation(), GetActorForwardVector());
	}
}

void AFlashlight::StopWeapon()
{
	UE_LOG(LogTemp, Warning, TEXT("Clearing Timer"));
	GetWorld()->GetTimerManager().ClearTimer(AttackTimerHandle);
	LightAttackComp->TurnOffLight();
}
void AFlashlight::ChangeWeaponAngle(float DeltaAngle)
{
	UE_LOG(LogTemp, Warning, TEXT("ChangeWeaponAngle Called"));
	LightAttackComp->ChangeLightAngle(DeltaAngle);
}
