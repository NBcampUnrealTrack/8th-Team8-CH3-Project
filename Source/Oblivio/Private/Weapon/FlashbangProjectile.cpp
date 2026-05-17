#include "Weapon/FlashbangProjectile.h"
#include "OblivioComponents/LightAttackComponent.h"
#include "Kismet/GameplayStatics.h"

AFlashbangProjectile::AFlashbangProjectile()
{
	LightAttackComp = CreateDefaultSubobject<ULightAttackComponent>(TEXT("LightAttackComp"));
	LightAttackComp->SetupAttachment(RootComponent);

	PrimaryActorTick.bCanEverTick = true;
	LightAttackComp->Damage = 10000;
	LightAttackComp->BasicLightColor = FColor::White;
	BangDelay = 2.0f;
	FlashDuration = 0.2f;
}

void AFlashbangProjectile::BeginPlay()
{
	Super::BeginPlay();
	UseWeapon();
	LightAttackComp->TurnOffLight();
}

void AFlashbangProjectile::UseWeapon()
{
	if (!IsValid(LightAttackComp)) return;
	if (!GetWorld()->GetTimerManager().IsTimerActive(BangTimerHandle)) {
		UE_LOG(LogTemp, Warning, TEXT("Setting Timer"));
		GetWorld()->GetTimerManager().SetTimer(
			BangTimerHandle,
			[this]() {
				FVector SourceLocation = LightAttackComp->GetComponentLocation();
				FVector LightDirection = LightAttackComp->GetForwardVector();
				// 단발 섬광 — 빛이 켜져 있는 시간(FlashDuration) 만큼을 노출량으로 한 번에 전달.
				LightAttackComp->CreateLightAttack(SourceLocation, LightDirection, FlashDuration);

				if (IsValid(ExplosionSound))
				{
					UGameplayStatics::PlaySoundAtLocation(this, ExplosionSound, GetActorLocation());
				}

			},
			BangDelay,
			false);
		GetWorld()->GetTimerManager().SetTimer(DestroyTimerHandle, this, &AFlashbangProjectile::StopWeapon, BangDelay + FlashDuration, false);
	}
}

void AFlashbangProjectile::StopWeapon()
{
	UE_LOG(LogTemp, Warning, TEXT("Stopping light, destroying weapon"));
	LightAttackComp->TurnOffLight();
	Destroy();
}

void AFlashbangProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	Super::EndPlay(EndPlayReason);
	GetWorld()->GetTimerManager().ClearTimer(BangTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(DestroyTimerHandle);
}