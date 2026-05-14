#include "OblivioCharacterController.h"
#include "OblivioCharacter.h"
#include "Crafting/OblivioCrafting.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Kismet/KismetMathLibrary.h"
#define ECC_Floor ECC_GameTraceChannel1

AOblivioCharacterController::AOblivioCharacterController()
	: DefaultMappingContext(nullptr)
	, InventoryMappingContext(nullptr)
	, MoveAction(nullptr)
	, LookAction(nullptr)
	, JumpAction(nullptr)
	, WheelAction(nullptr)
	, RunAction(nullptr)
	, FlashlightToggleAction(nullptr)
	, FlashbangAction(nullptr)
	, FlareAction(nullptr)
	, ReloadAction(nullptr)
	, InventoryAction(nullptr)
	, CraftingAction(nullptr)
	, RotateAction(nullptr)
	, PlaceObstacleAction(nullptr)
	, InteractAction(nullptr)
	, PauseAction(nullptr)
	, CraftingMappingContext(nullptr)
	, SelectObstacleAction(nullptr)

{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
}

void AOblivioCharacterController::BeginPlay()
{
	Super::BeginPlay();

	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
		{
			if (DefaultMappingContext)
			{
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}
}

void AOblivioCharacterController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (UEnhancedInputComponent* EIC = CastChecked<UEnhancedInputComponent>(InputComponent))
	{
		EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AOblivioCharacterController::OnMove);
		EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &AOblivioCharacterController::OnLook);
		EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &AOblivioCharacterController::OnJumpStarted);
		EIC->BindAction(JumpAction, ETriggerEvent::Completed, this, &AOblivioCharacterController::OnJumpCompleted);
		EIC->BindAction(WheelAction, ETriggerEvent::Triggered, this, &AOblivioCharacterController::OnWheel);
		EIC->BindAction(RunAction, ETriggerEvent::Started, this, &AOblivioCharacterController::OnRunStarted);
		EIC->BindAction(RunAction, ETriggerEvent::Completed, this, &AOblivioCharacterController::OnRunCompleted);
		EIC->BindAction(FlashlightToggleAction, ETriggerEvent::Started, this, &AOblivioCharacterController::OnFlashlightToggle);
		EIC->BindAction(FlashbangAction, ETriggerEvent::Started, this, &AOblivioCharacterController::OnFlashbang);
		EIC->BindAction(FlareAction, ETriggerEvent::Started, this, &AOblivioCharacterController::OnFlare);
		EIC->BindAction(ReloadAction, ETriggerEvent::Started, this, &AOblivioCharacterController::OnReload);
		EIC->BindAction(InventoryAction, ETriggerEvent::Started, this, &AOblivioCharacterController::OnInventoryToggle);
		EIC->BindAction(CraftingAction, ETriggerEvent::Started, this, &AOblivioCharacterController::OnCraftingToggle);
		EIC->BindAction(RotateAction, ETriggerEvent::Started, this, &AOblivioCharacterController::OnRotatePreview);
		EIC->BindAction(PlaceObstacleAction, ETriggerEvent::Started, this, &AOblivioCharacterController::OnPlaceObstacle);
		//EIC->BindAction(SelectObstacleAction, ETriggerEvent::Started, this, &AOblivioCharacterController::OnSelectObstacle);
		EIC->BindAction(SelectObstacleAction, ETriggerEvent::Triggered, this, &AOblivioCharacterController::OnSelectObstacle);
		EIC->BindAction(InteractAction, ETriggerEvent::Started, this, &AOblivioCharacterController::OnInteract);
		EIC->BindAction(PauseAction, ETriggerEvent::Started, this, &AOblivioCharacterController::OnPauseToggle);
	}
}

void AOblivioCharacterController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	if (AOblivioCharacter* ObjChar = Cast<AOblivioCharacter>(GetPawn()))
	{
		if (ObjChar->IsAlive()) // 캐릭터가 죽지 않았다면 회전 업데이트
		{
			UpdateMouseRotation(DeltaTime);
		}
	}
}

void AOblivioCharacterController::UpdateMouseRotation(float DeltaTime)
{
	if (APawn* MyPawn = GetPawn())
	{
		FHitResult Hit;
		if (GetHitResultUnderCursor(ECC_Visibility, false, Hit))
		{
			// 목표 회전값
			FRotator TargetRot = UKismetMathLibrary::FindLookAtRotation(MyPawn->GetActorLocation(), Hit.ImpactPoint);
			TargetRot.Pitch = 0.f; // 위아래로 기울지 않도록 고정
			TargetRot.Roll = 0.f;

			// 현재 회전값
			FRotator CurrentRot = MyPawn->GetActorRotation();

			// 회전 속도
			float RotationSpeed = 5.0f;

			FRotator SmoothRot = FMath::RInterpTo(CurrentRot, TargetRot, DeltaTime, RotationSpeed);

			MyPawn->SetActorRotation(SmoothRot);
		}
	}
}

void AOblivioCharacterController::OnMove(const FInputActionValue& Value)
{
	if (AOblivioCharacter* ObjChar = Cast<AOblivioCharacter>(GetPawn()))
		ObjChar->Move(Value.Get<FVector2D>());
}

void AOblivioCharacterController::OnLook(const FInputActionValue& Value)
{
	// Tick에서 자동 처리 중
}

void AOblivioCharacterController::OnJumpStarted(const FInputActionValue& Value)
{
	if (AOblivioCharacter* ObjChar = Cast<AOblivioCharacter>(GetPawn()))
	{
		ObjChar->Jump();
	}
}

void AOblivioCharacterController::OnJumpCompleted(const FInputActionValue& Value)
{
	if (AOblivioCharacter* ObjChar = Cast<AOblivioCharacter>(GetPawn()))
	{
		ObjChar->StopJumping();
	}
}

void AOblivioCharacterController::OnWheel(const FInputActionValue& Value)
{
	if (AOblivioCharacter* ObjChar = Cast<AOblivioCharacter>(GetPawn()))
		ObjChar->AdjustFocus(Value.Get<float>());
}

void AOblivioCharacterController::OnRunStarted(const FInputActionValue& Value)
{
	if (AOblivioCharacter* ObjChar = Cast<AOblivioCharacter>(GetPawn()))
		ObjChar->StartRunning();
}

void AOblivioCharacterController::OnRunCompleted(const FInputActionValue& Value)
{
	if (AOblivioCharacter* ObjChar = Cast<AOblivioCharacter>(GetPawn()))
		ObjChar->StopRunning();
}

void AOblivioCharacterController::OnFlashlightToggle(const FInputActionValue& Value)
{
	if (AOblivioCharacter* ObjChar = Cast<AOblivioCharacter>(GetPawn()))
		ObjChar->ToggleFlashlight();
}

void AOblivioCharacterController::OnFlashbang(const FInputActionValue& Value)
{
	if (AOblivioCharacter* ObjChar = Cast<AOblivioCharacter>(GetPawn()))
		if (IsValid(ObjChar->FlashbangWeapon))
			ObjChar->BeginThrow(ObjChar->FlashbangWeapon);
}
void AOblivioCharacterController::OnFlare(const FInputActionValue& Value)
{
	if (AOblivioCharacter* ObjChar = Cast<AOblivioCharacter>(GetPawn()))
		if (IsValid(ObjChar->FlareWeapon))
			ObjChar->BeginThrow(ObjChar->FlareWeapon);
}

void AOblivioCharacterController::OnReload(const FInputActionValue& Value)
{
	if (AOblivioCharacter* ObjChar = Cast<AOblivioCharacter>(GetPawn()))
	{
		ObjChar->ReloadBattery();
	}
}

void AOblivioCharacterController::OnInventoryToggle(const FInputActionValue& Value)
{
	if (AOblivioCharacter* ObjChar = Cast<AOblivioCharacter>(GetPawn()))
		ObjChar->ToggleInventory();
}

void AOblivioCharacterController::OnCraftingToggle(const FInputActionValue& Value)
{
	if (AOblivioCharacter* ObjChar = Cast<AOblivioCharacter>(GetPawn()))
	{
		ObjChar->ToggleCrafting(); // 캐릭터 상태 토글

		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			if (ObjChar->bIsCraftingOpen) // 크래프팅 창이 열리면
			{
				Subsystem->AddMappingContext(CraftingMappingContext, 1); // 높은 우선순위로 추가
			}
			else
			{
				Subsystem->RemoveMappingContext(CraftingMappingContext);
			}
		}
	}
}

void AOblivioCharacterController::OnRotatePreview(const FInputActionValue& Value)
{
	if (AOblivioCharacter* ObjChar = Cast<AOblivioCharacter>(GetPawn()))
	{
		if (ObjChar->CraftingComponent)
		{
			ObjChar->CraftingComponent->RotatePreview();
		}
	}
}

void AOblivioCharacterController::OnSelectObstacle(const FInputActionValue& Value)
{
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::White, TEXT("Number Key Pressed"));
	
	// 입력값(1~7)을 받아 크래프팅 컴포넌트로 전달
	float Magnitude = Value.Get<float>();
	int32 SelectedIndex = FMath::RoundToInt(Magnitude);

	if (CurrentSelectedIndex == SelectedIndex) return;
	CurrentSelectedIndex = SelectedIndex;
	
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::White,
		FString::Printf(TEXT("Raw: %f | Int: %d"), Magnitude, SelectedIndex));
	if (SelectedIndex <= 0)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Orange, TEXT("Warning: Input is 0. Check IMC Scalar settings!"));
		SelectedIndex = 1;
	}

	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::White,
		FString::Printf(TEXT("Input Value: %f -> Index: %d"), Magnitude, SelectedIndex));

	if (AOblivioCharacter* ObjChar = Cast<AOblivioCharacter>(GetPawn()))
	{
		if (auto* CraftingComp = ObjChar->FindComponentByClass<UOblivioCrafting>())
		{
			CraftingComp->SelectObstacle(SelectedIndex);

			CurrentSelectedIndex = 0;
		}
	}
}

void AOblivioCharacterController::OnPlaceObstacle(const FInputActionValue& Value)
{
	if (AOblivioCharacter* ObjChar = Cast<AOblivioCharacter>(GetPawn()))
	{
		if (auto* CraftingComp = ObjChar->FindComponentByClass<UOblivioCrafting>())
		{
			CraftingComp->PlaceObstacle();
		}
	}
}
void AOblivioCharacterController::OnInteract(const FInputActionValue& Value)
{
	if (AOblivioCharacter* ObjChar = Cast<AOblivioCharacter>(GetPawn()))
		ObjChar->Interact();
}
void AOblivioCharacterController::OnPauseToggle(const FInputActionValue& Value)
{
	if (AOblivioCharacter* ObjChar = Cast<AOblivioCharacter>(GetPawn()))
	{
		ObjChar->TogglePause();
	}
}