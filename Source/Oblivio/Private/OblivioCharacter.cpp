#include "OblivioCharacter.h"
#include "OblivioGameMode.h"
#include "OblivioGameInstance.h"
#include "Weapon/WeaponBase.h"
#include "Weapon/ThrowableWeapon.h"
#include "Crafting/OblivioCrafting.h"
#include "Items/OblivioItemBase.h"
#include "OblivioComponents/SoundPropagationComponent.h"
#include "OblivioComponents/PlayerCombatComponent.h"
#include "Items/OblivioInventoryComponent.h"

#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraComponent.h"
#include "Engine/DamageEvents.h"

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

	InventoryComponent = CreateDefaultSubobject<UOblivioInventoryComponent>(TEXT("InventoryComponent"));

	WheelControlMultiplier = 3.f;
}

void AOblivioCharacter::BeginPlay()
{
	Super::BeginPlay();
	UpdateFlashlightVisuals();

	//8층부터는 현재 스탯을 인스턴스에 저장된 것으로 대체
	UOblivioGameInstance* GI = Cast<UOblivioGameInstance>(GetGameInstance());
	if (!GI) return;

	if (GI->CurrentFloor < 9)
	{
		CurrentHealth = GI->CurrentHealth;
		Battery = GI->CurrentBattery;
		Hunger = GI->CurrentHunger;
		Thirst = GI->CurrentThirst;
	}

	//시작시 손전등 장착
	if (IsValid(FlashlightWeapon)) {
		UE_LOG(LogTemp, Warning, TEXT("Spawning Weapon"));
		FActorSpawnParameters Params;
		Params.Owner = this;
		CurrentWeapon = GetWorld()->SpawnActor<AWeaponBase>(FlashlightWeapon, GetActorTransform(), Params);
		if (IsValid(CurrentWeapon)) {
			UE_LOG(LogTemp, Warning, TEXT("Attaching Weapon"));
			CurrentWeapon->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, FName("WeaponSocket"));
		}

	}
	FlashlightComponent->SetVisibility(false);
}

//==========================
// Tick and Status
//==========================

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

void AOblivioCharacter::UpdateStatus(float DeltaTime)
{
	if (bIsDead) return;

	float DepleteRate = bIsRunning ? 2.0f : 1.0f;
	Hunger = FMath::Max(0.0f, Hunger - (DeltaTime * 0.3f * DepleteRate));
	Thirst = FMath::Max(0.0f, Thirst - (DeltaTime * 0.4f * DepleteRate));

	// 배터리 처리
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

	// 굶주림/갈증으로 인한 체력 감소
	if (Hunger <= 0.0f || Thirst <= 0.0f)
	{
		ApplyHealth(DeltaTime * 1.0f);
	}

	GetCharacterMovement()->MaxWalkSpeed = bIsRunning ? RunSpeed : WalkSpeed;
}

//==========================
// 입력 and 상호작용
//==========================

void AOblivioCharacter::Move(const FVector2D& Value)
{
	if (Controller != nullptr)
	{
		const float Dir = bMovementInverted ? -1.0f : 1.0f;
		AddMovementInput(FVector::ForwardVector, Value.Y * Dir);
		AddMovementInput(FVector::RightVector,   Value.X * Dir);
	}
}

void AOblivioCharacter::StartRunning() { bIsRunning = true; }
void AOblivioCharacter::StopRunning() { bIsRunning = false; }


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

	DrawDebugLine(GetWorld(), Start, End, FColor::Red, false, 1.0f, 0, 2.0f);

	if (CurrentNearbyItem)
	{
		if (InventoryComponent && InventoryComponent->AddItem(CurrentNearbyItem))
		{
			CurrentNearbyItem->Destroy();
			SetNearbyItem(nullptr);
			return; // 습득 성공 시 종료
		}
	}

	if (GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, Params))
	{
		
		//추가: 열쇠/유품 획득 시 정보 저장
		AActor* HitActor = HitResult.GetActor();
		UE_LOG(LogTemp, Warning, TEXT("1. Hit Something: %s"), *HitActor->GetName());

		if (AOblivioItemBase* PickedItem = Cast<AOblivioItemBase>(HitActor))
		{
			if (InventoryComponent && InventoryComponent->AddItem(PickedItem))
			{
				PickedItem->Destroy();
				UE_LOG(LogTemp, Warning, TEXT("3. Item Destroyed!"));
				return;
			}
		}

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
		//체크포인트 상호작용 시 저장
		else if (HitActor->ActorHasTag("RestArea")) //체크포인트 태그 확인 후 태그명 수정 필요
		{
			GM->RestInteraction();
				//아래에 체크포인트 시 회복 등 넣을 수 있음
		}
		//----
	}
}

void AOblivioCharacter::SetNearbyItem(AOblivioItemBase* Item)
{
	CurrentNearbyItem = Item;
	// UI 바인딩 필요 press e
	OnNearbyItemChanged.Broadcast(CurrentNearbyItem);
}

void AOblivioCharacter::TogglePause()
{
	//UI 띄우는 로직 연동
	UE_LOG(LogTemp, Warning, TEXT("Pause Menu Toggled!"));
	
	bIsPauseOpen = !bIsPauseOpen;
	
	OnPauseToggle(bIsPauseOpen);

	// 만약 직접 엔진 일시정지 제어 시
	/*
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC)
	{
		bool bIsPaused = UGameplayStatics::IsGamePaused(GetWorld());
		UGameplayStatics::SetGamePaused(GetWorld(), !bIsPaused);
		PC->SetShowMouseCursor(!bIsPaused);
	}
	*/
}

//=====================
//무기 장비
//======================

void AOblivioCharacter::AdjustFocus(float Value)
{
	if (bCanAdjustFocus && CurrentWeapon)
	{
		CurrentWeapon->ChangeWeaponAngle(Value * WheelControlMultiplier);
	}
}


void AOblivioCharacter::ToggleFlashlight()
{
	if (bFlashlightForcedOff) return;   // 암전 효과 진행 중 — 토글 차단
	if (Battery > 0.0f)
	{
		bIsFlashlightOn = !bIsFlashlightOn;
		UpdateFlashlightVisuals();
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
}


void AOblivioCharacter::ReloadBattery()
{
	if (bIsDead) return;

	//인벤토리/배터리 아이템 생길 시 조건추가.
	bool bHasBatteryItem = true; // 현재 있다고 가정

	if (bHasBatteryItem)
	{
		if (Battery >= 100.0f)
		{
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, TEXT("Battery is already full."));
			return;
		}

		// 충전 로직
		Battery = 100.0f;

		if (!bIsFlashlightOn)
		{
			bIsFlashlightOn = true;
			UpdateFlashlightVisuals();
		}

		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, TEXT("Flashlight Recharged!"));
	}
}

void AOblivioCharacter::UseFlashbang()//섬광탄 무기 투척으로 변경
{
	ThrowWeapon(FlashbangWeapon);
}

void AOblivioCharacter::UseFlare()
{
	UE_LOG(LogTemp, Warning, TEXT("Using Flare"));
	ThrowWeapon(FlareWeapon);
	// 소리 자극은 ThrowableWeapon::OnLanded 에서 착지 위치 기준으로 전파한다.
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
	if(ThrowingWeapon) ThrowingWeapon->StartThrow(GetAimingLocation());
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

//========================
// 전투 and 피해
//========================

void AOblivioCharacter::ApplyHealth(float Damage)
{
	if (bIsDead) return;

	// 체력을 차감하고 최소값을 0으로 유지
	CurrentHealth = FMath::Clamp(CurrentHealth - Damage, 0.0f, MaxHealth);

	if (CurrentHealth <= 0.0f)
	{
		HandleDeath();
	}
}

float AOblivioCharacter::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	const float AppliedDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	ApplyHealth(AppliedDamage); // 통합된 데미지 처리 함수 호출

	// 블루프린트나 UI 갱신을 위해 델리게이트 방송
	OnPlayerDamaged.Broadcast(AppliedDamage, CurrentHealth, MaxHealth);

	return AppliedDamage;
}

void AOblivioCharacter::HandleDeath()
{
	if (bIsDead) return;

	bIsDead = true;
	DisableInput(Cast<APlayerController>(GetController()));

	if (AOblivioGameMode* GM = Cast<AOblivioGameMode>(UGameplayStatics::GetGameMode(GetWorld())))
	{
		GM->GameOver();
	}
}

bool AOblivioCharacter::IsAlive() const
{
	return (CurrentHealth > 0.0f && !bIsDead);
}

void AOblivioCharacter::ApplyCCSlow(float SpeedMultiplier, float Duration)
{
	// 슬로우 로직 구현
}

void AOblivioCharacter::ApplyCCStun(float Duration)
{
	// 스턴 로직 구현
}

void AOblivioCharacter::ApplyFlashlightBlackout(float Duration)
{
	if (Duration <= 0.0f) return;

	bFlashlightForcedOff = true;

	// 켜져 있으면 즉시 끄기
	if (bIsFlashlightOn)
	{
		bIsFlashlightOn = false;
		UpdateFlashlightVisuals();
	}

	// Duration 후 강제 해제
	GetWorldTimerManager().SetTimer(
		FlashlightBlackoutTimer,
		[this]()
		{
			bFlashlightForcedOff = false;
		},
		Duration, /*bLoop=*/false);
}

void AOblivioCharacter::ApplyMovementInversion(float Duration)
{
	if (Duration <= 0.0f) return;

	bMovementInverted = true;

	GetWorldTimerManager().SetTimer(
		MovementInversionTimer,
		[this]()
		{
			bMovementInverted = false;
		},
		Duration, /*bLoop=*/false);
}