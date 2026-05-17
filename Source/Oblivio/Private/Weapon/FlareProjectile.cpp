//FlareProjectile.cpp


#include "Weapon/FlareProjectile.h"
#include "OblivioComponents/LightAttackComponent.h"

AFlareProjectile::AFlareProjectile()
{
	LightAttackComp = CreateDefaultSubobject<ULightAttackComponent>(TEXT("LightAttackComp"));
	LightAttackComp->SetupAttachment(RootComponent);

	PrimaryActorTick.bCanEverTick = true;
	AttackInterval = 0.1f;
	LastDuration = 5.f;
	LightAttackComp->Damage = 1;
	LightAttackComp->BasicLightColor = FColor::Red;
}

void AFlareProjectile::BeginPlay()
{
	Super::BeginPlay();
	UseWeapon();
}

void AFlareProjectile::UseWeapon()
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
				// 누적형 기믹용 노출 시간(이번 틱 = AttackInterval) 동봉.
				LightAttackComp->CreateLightAttack(SourceLocation, LightDirection, AttackInterval); },
			AttackInterval,
			true);
		GetWorld()->GetTimerManager().SetTimer(DestroyTimerHandle, this, &AFlareProjectile::StopWeapon, LastDuration, false);
	}
}

void AFlareProjectile::StopWeapon()
{
	LightAttackComp->TurnOffLight();
	if (GetWorld()->GetTimerManager().IsTimerActive(AttackTimerHandle)) {
		GetWorld()->GetTimerManager().ClearTimer(AttackTimerHandle);
	}
	Destroy();
}

void AFlareProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	Super::EndPlay(EndPlayReason);
	GetWorld()->GetTimerManager().ClearTimer(AttackTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(DestroyTimerHandle);
}