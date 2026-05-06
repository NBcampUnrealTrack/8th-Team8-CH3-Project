// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon/Flashbang.h"
#include "OblivioComponents/LightAttackComponent.h"

AFlashbang::AFlashbang()
{
	PrimaryActorTick.bCanEverTick = true;
	LightAttackComp->Damage = 10000;
	LightAttackComp->BasicLightColor = FColor::White;
	BangDelay = 2.0f;
	FlashDuration = 0.2f;
}

void AFlashbang::BeginPlay()
{
	Super::BeginPlay();
	UseWeapon();
	LightAttackComp->TurnOffLight();
}

void AFlashbang::UseWeapon()
{
	if (!IsValid(LightAttackComp)) return;
	if (!GetWorld()->GetTimerManager().IsTimerActive(BangTimerHandle)) {
		UE_LOG(LogTemp, Warning, TEXT("Setting Timer"));
		GetWorld()->GetTimerManager().SetTimer(
			BangTimerHandle,
			[this]() {
				FVector SourceLocation = LightAttackComp->GetComponentLocation();
				FVector LightDirection = LightAttackComp->GetForwardVector();
				LightAttackComp->CreateLightAttack(SourceLocation, LightDirection); },
			BangDelay,
			false);
		GetWorld()->GetTimerManager().SetTimer(DestroyTimerHandle, this, &AFlashbang::StopWeapon, BangDelay + FlashDuration, false);
	}
}

void AFlashbang::StopWeapon()
{
	UE_LOG(LogTemp, Warning, TEXT("Stopping light, destroying weapon"));
	LightAttackComp->TurnOffLight();
	Destroy();
}
