#include "OblivioCharacter.h"
#include "OblivioGameMode.h"
#include "Weapon/WeaponBase.h"
#include "Weapon/ThrowableWeapon.h"
#include "Crafting/OblivioCrafting.h"
#include "OblivioComponents/SoundPropagationComponent.h"
#include "OblivioComponents/PlayerCombatComponent.h"

#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraComponent.h"

AOblivioCharacter::AOblivioCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->SetRelativeRotation(FRotator(-65.f, 0.f, 0.f));
	CameraBoom->TargetArmLength = 900.f;
	CameraBoom->bInheritYaw = false;

	TopDownCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
	TopDownCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);

	FlashlightComponent = CreateDefaultSubobject<USpotLightComponent>(TEXT("Flashlight"));
	FlashlightComponent->SetupAttachment(RootComponent);
	FlashlightComponent->SetRelativeLocation(FVector(40.f, 0.f, 40.f));

	FlashbangLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FlashbangLight"));
	FlashbangLight->SetupAttachment(RootComponent);
	FlashbangLight->SetIntensity(0.0f); // 평소에는 꺼둠
	FlashbangLight->SetCastShadows(false);
	FlashbangLight->SetAttenuationRadius(800.0f);

	CraftingComponent = CreateDefaultSubobject<UOblivioCrafting>(TEXT("CraftingComponent"));

	SoundPropagationComp = CreateDefaultSubobject<USoundPropagationComponent>(TEXT("SoundPropagationComp"));

	CombatComp = CreateDefaultSubobject<UPlayerCombatComponent>(TEXT("CombatComp"));

	GetCharacterMovement()->bOrientRotationToMovement = false;
	bUseControllerRotationYaw = false;

	WheelControlMultiplier = 3.f;
}

void AOblivioCharacter::BeginPlay()
{
	Super::BeginPlay();
	UpdateFlashlightVisuals();

	//시작시 손전등 장착
	if (IsValid(FlashlightWeapon)) {
		UE_LOG(LogTemp, Warning, TEXT("Spawning Weapon"));
		FActorSpawnParameters Params;
		Params.Owner = this;
		CurrentWeapon = GetWorld()->SpawnActor<AWeaponBase>(FlashlightWeapon, GetActorTransform(), Params);
		if (IsValid(CurrentWeapon)) {
			UE_LOG(LogTemp, Warning, TEXT("Attaching Weapon"));
			CurrentWeapon->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, FName("WeaponSocket"));
			CurrentWeapon->AddActorLocalRotation(FRotator(0,90,0));
		}
	}
	FlashlightComponent->SetVisibility(false);
}

void AOblivioCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateStatus(DeltaTime);
	//Debug 확인용
	if (GEngine)
	{
		//생존 스탯 (Health, Hunger, Thirst)
		FString StatusMsg = FString::Printf(TEXT("HP: %.1f | Hunger: %.1f | Thirst: %.1f"), CurrentHealth, Hunger, Thirst);
		GEngine->AddOnScreenDebugMessage(1, DeltaTime, FColor::Cyan, StatusMsg);

		//배터리 상태 및 손전등 ON/OFF
		FString BatteryMsg = FString::Printf(TEXT("Battery: %.1f%% (%s) | Focus: %.2f"),
			Battery, bIsFlashlightOn ? TEXT("ON") : TEXT("OFF"), CurrentFocusAlpha);

		// 배터리가 적으면 빨간색, 충분하면 초록색으로 표시
		FColor BatteryColor = (Battery < 20.f) ? FColor::Red : FColor::Green;
		GEngine->AddOnScreenDebugMessage(2, DeltaTime, BatteryColor, BatteryMsg);

		//이동 상태
		FString MoveMsg = FString::Printf(TEXT("Movement: %s | Speed: %.1f"),
			bIsRunning ? TEXT("RUNNING") : TEXT("WALKING"), GetVelocity().Size());
		GEngine->AddOnScreenDebugMessage(3, DeltaTime, FColor::Yellow, MoveMsg);
	}

	if (IsValid(CurrentWeapon)) {
		CurrentWeapon->SetActorRotation(GetActorRotation());
	}
}

void AOblivioCharacter::Move(const FVector2D& Value)
{
	if (Controller != nullptr)
	{
		AddMovementInput(FVector::ForwardVector, Value.Y);
		AddMovementInput(FVector::RightVector, Value.X);
	}
}

void AOblivioCharacter::AdjustFocus(float Value)
{
	if (!bCanAdjustFocus) return;
	/*
	CurrentFocusAlpha = FMath::Clamp(CurrentFocusAlpha + (Value * 0.1f), 0.0f, 1.0f);
	UpdateFlashlightVisuals();*/
	CurrentWeapon->ChangeWeaponAngle(Value * WheelControlMultiplier);
}

void AOblivioCharacter::StartRunning() { bIsRunning = true; }
void AOblivioCharacter::StopRunning() { bIsRunning = false; }

void AOblivioCharacter::UseFlashbang()//섬광탄 무기 투척으로 변경
{
	ThrowWeapon(FlashbangWeapon);
	/*
	if (Battery >= 50.0f)
	{
		Battery -= 50.0f;

		FlashbangIntensity = 50000.0f;
		FlashbangLight->SetIntensity(FlashbangIntensity);
		FlashbangLight->SetVisibility(true);

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::White, TEXT("!!! FLASHBANG !!!"));
		}

		GetWorldTimerManager().SetTimer(FlashbangTimerHandle, this, &AOblivioCharacter::FadeOutFlashbang, 0.01f, true);

		// 일정 시간 동안 소리가 안 들리는 연출이나 몬스터 소멸 로직 호출
	}*/
}
void AOblivioCharacter::UseFlare()
{
	UE_LOG(LogTemp, Warning, TEXT("Using Flare"));
	ThrowWeapon(FlareWeapon);
	SoundPropagationComp->PropagateSound();
}
void AOblivioCharacter::ThrowWeapon(TSubclassOf<AThrowableWeapon> Weapon) {
	if (!IsValid(Weapon)) {
		UE_LOG(LogTemp, Warning, TEXT("ThrowWeapon invalid call!"));
		return;
	}
	FActorSpawnParameters Params;
	Params.Owner = this;
	AThrowableWeapon* ThrowingWeapon = GetWorld()->SpawnActor<AThrowableWeapon>(
		Weapon,
		GetActorLocation(),
		FRotator::ZeroRotator,
		Params);
	FVector temp = GetAimingLocation();
	UE_LOG(LogTemp, Warning, TEXT("Throwing Weapon %s to %f %f!"), *ThrowingWeapon->GetName(), temp.X, temp.Y);
	ThrowingWeapon->StartThrow(GetAimingLocation());
}

FVector AOblivioCharacter::GetAimingLocation() {
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!IsValid(PC)) return FVector::ZeroVector;
	FHitResult HitResult;
	PC->GetHitResultUnderCursor(
		ECC_Visibility,
		false,
		HitResult
	);

	return HitResult.Location;
}

void AOblivioCharacter::FadeOutFlashbang()
{
	// 빛의 강도를 서서히 줄임
	FlashbangIntensity -= 2000.0f;

	if (FlashbangIntensity <= 0.0f)
	{
		FlashbangIntensity = 0.0f;
		FlashbangLight->SetIntensity(0.0f);
		FlashbangLight->SetVisibility(false);

		// 타이머 종료
		GetWorldTimerManager().ClearTimer(FlashbangTimerHandle);
	}
	else
	{
		FlashbangLight->SetIntensity(FlashbangIntensity);
	}
}

void AOblivioCharacter::ToggleFlashlight()
{
	if (Battery > 0.0f)
	{
		bIsFlashlightOn = !bIsFlashlightOn;
		UpdateFlashlightVisuals();
	}
}

void AOblivioCharacter::ToggleInventory()
{
	bIsInventoryOpen = !bIsInventoryOpen;
}

void AOblivioCharacter::ToggleCrafting()
{
	if (CraftingComponent)
	{
		CraftingComponent->ToggleCraftingMode();
		bIsCraftingOpen = CraftingComponent->bIsCraftingModeActive;
	}
}

void AOblivioCharacter::Interact()
{
	FHitResult HitResult;
	FVector Start = GetActorLocation();
	FVector End = Start + (GetActorForwardVector() * InteractionDistance);
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	if (GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, Params))
	{
		UE_LOG(LogTemp, Warning, TEXT("Interacted with: %s"), *HitResult.GetActor()->GetName());
		//추가: 열쇠/유품 획득 시 정보 저장
		AActor* HitActor = HitResult.GetActor();
		AOblivioGameMode* GM = Cast<AOblivioGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
		if (!GM) return;
		if (HitActor->ActorHasTag("Key"))
		{
			GM->CollectedKeys++;
			UE_LOG(LogTemp, Warning, TEXT("You Get a Key! Current: %d / %d"), GM->CollectedKeys, GM->RequiredKeys);
			HitActor->Destroy();
		}
		else if (HitActor->ActorHasTag("Memento"))
		{
			GM->AddMemento();
			UE_LOG(LogTemp, Warning, TEXT("Get Memento!"));
			HitActor->Destroy();
		}
		//----
	}
}

void AOblivioCharacter::UpdateStatus(float DeltaTime)
{
	float DepleteRate = bIsRunning ? 2.0f : 1.0f;
	Hunger = FMath::Max(0.0f, Hunger - (DeltaTime * 0.3f * DepleteRate));
	Thirst = FMath::Max(0.0f, Thirst - (DeltaTime * 0.4f * DepleteRate));

	if (bIsFlashlightOn && Battery > 0.0f)
	{
		float FocusPenalty = FMath::Lerp(1.0f, 1.5f, CurrentFocusAlpha);
		Battery = FMath::Max(0.0f, Battery - (DeltaTime * BatteryDepletionRate * FocusPenalty));

		if (Battery <= 0.0f)
		{
			bIsFlashlightOn = false;
			UpdateFlashlightVisuals();
		}
	}

	if (Hunger <= 0.0f || Thirst <= 0.0f) CurrentHealth -= DeltaTime * 1.0f;
	GetCharacterMovement()->MaxWalkSpeed = bIsRunning ? RunSpeed : WalkSpeed;

	//추가: 체력 0이 되면 게임오버
	if (CurrentHealth <= 0 && bIsDead == false)
	{
		bIsDead = true;
		DisableInput(Cast<APlayerController>(GetController()));
		AOblivioGameMode* GM = Cast<AOblivioGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
		if (!GM) return;
		GM->GameOver();
	}
}

void AOblivioCharacter::UpdateFlashlightVisuals()
{
	if (!IsValid(CurrentWeapon)) return;

	if (bIsFlashlightOn) {	//On
		CurrentWeapon->UseWeapon();
	}
	else {	//Off
		CurrentWeapon->StopWeapon();
	}
	/*
	if (!FlashlightComponent) return;
	FlashlightComponent->SetVisibility(bIsFlashlightOn);
	if (bIsFlashlightOn)
	{
		float TargetAngle = FMath::Lerp(60.0f, 15.0f, CurrentFocusAlpha);
		float TargetRadius = FMath::Lerp(600.0f, 1800.0f, CurrentFocusAlpha);
		FlashlightComponent->SetOuterConeAngle(TargetAngle);
		FlashlightComponent->SetAttenuationRadius(TargetRadius);
	}*/
}

void AOblivioCharacter::ReloadBattery()
{
}

void AOblivioCharacter::HandleDeath()
{
}

//체력 적용
void AOblivioCharacter::ApplyHealth(float Damage)
{
	UE_LOG(LogTemp, Warning, TEXT("%f damage to the player!"), Damage);
	CurrentHealth -= Damage;
	if (CurrentHealth <= 0) {
		UE_LOG(LogTemp, Warning, TEXT("player health is below 0!"), Damage);
	}
	return;
}

//슬로우 적용
void AOblivioCharacter::ApplyCCSlow(float SpeedMultiplier, float Duration) {
	UE_LOG(LogTemp, Warning, TEXT("slow to the player!"));
	//슬로우 적용할거면 구현
}

//스턴 적용
void AOblivioCharacter::ApplyCCStun(float Duration) {
	UE_LOG(LogTemp, Warning, TEXT("Stun the player!"));
	//스턴 적용할거면 구현
}

//생존여부 반환
bool AOblivioCharacter::IsAlive() const {	
	if (CurrentHealth >= 0) {
		UE_LOG(LogTemp, Warning, TEXT("Player Alive!"));
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("Player Dead!"));
	}
	return CurrentHealth >= 0;
	
}
//플레이어 피격 호출
float AOblivioCharacter::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	const float AppliedDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	UE_LOG(LogTemp, Warning, TEXT("Player is taking %f Damage!"), DamageAmount);
	OnPlayerDamaged.Broadcast(AppliedDamage, CurrentHealth, MaxHealth);
	return DamageAmount;
}
