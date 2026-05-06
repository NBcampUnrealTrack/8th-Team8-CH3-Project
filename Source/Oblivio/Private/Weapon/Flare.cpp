// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon/Flare.h"
#include "OblivioComponents/LightAttackComponent.h"

AFlare::AFlare()
{
	PrimaryActorTick.bCanEverTick = true;
	AttackInterval = 0.1f;
	LastDuration = 5.f;
	LightAttackComp->Damage = 1;
	LightAttackComp->BasicLightColor = FColor::Red;
}

void AFlare::BeginPlay()
{
	Super::BeginPlay();
	UseWeapon();
}

void AFlare::UseWeapon()
{
	UE_LOG(LogTemp, Warning, TEXT("Flare use weapon called"));
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
		GetWorld()->GetTimerManager().SetTimer(DestroyTimerHandle, this, &AFlare::StopWeapon, LastDuration, false);
	}
}

void AFlare::StopWeapon()
{
	LightAttackComp->TurnOffLight();
	if (GetWorld()->GetTimerManager().IsTimerActive(AttackTimerHandle)) {
		GetWorld()->GetTimerManager().ClearTimer(AttackTimerHandle);
	}
	Destroy();
}
