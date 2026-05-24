#include "OblivioCharacterController.h"
#include "OblivioCharacter.h"
#include "OblivioGameUserSettings.h"
#include "Crafting/OblivioCrafting.h"
#include "EnhancedInputComponent.h"
#include "Items/OblivioInventoryComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
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
	, CrouchAction(nullptr)
	, FlashlightToggleAction(nullptr)
	, FlashbangAction(nullptr)
	, ThrowBottleAction(nullptr)
	, ReloadAction(nullptr)
	, InventoryAction(nullptr)
	, CraftingAction(nullptr)
	, RotateAction(nullptr)
	, PlaceObstacleAction(nullptr)
	, InteractAction(nullptr)
	, PauseAction(nullptr)
	, QuickEatFoodAction(nullptr)
	, QuickDrinkWaterAction(nullptr)
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
		EIC->BindAction(CrouchAction, ETriggerEvent::Started, this, &AOblivioCharacterController::OnCrouchStarted);
		EIC->BindAction(CrouchAction, ETriggerEvent::Completed, this, &AOblivioCharacterController::OnCrouchCompleted);
		EIC->BindAction(FlashlightToggleAction, ETriggerEvent::Started, this, &AOblivioCharacterController::OnFlashlightToggle);
		EIC->BindAction(FlashbangAction, ETriggerEvent::Started, this, &AOblivioCharacterController::OnFlashbang);
		EIC->BindAction(ThrowBottleAction, ETriggerEvent::Started, this, &AOblivioCharacterController::OnThrowBottle);
		EIC->BindAction(ReloadAction, ETriggerEvent::Started, this, &AOblivioCharacterController::OnReload);
		EIC->BindAction(InventoryAction, ETriggerEvent::Started, this, &AOblivioCharacterController::OnInventoryToggle);
		EIC->BindAction(CraftingAction, ETriggerEvent::Started, this, &AOblivioCharacterController::OnCraftingToggle);
		EIC->BindAction(RotateAction, ETriggerEvent::Started, this, &AOblivioCharacterController::OnRotatePreview);
		EIC->BindAction(PlaceObstacleAction, ETriggerEvent::Started, this, &AOblivioCharacterController::OnPlaceObstacle);
		EIC->BindAction(QuickEatFoodAction, ETriggerEvent::Started, this, &AOblivioCharacterController::OnQuickEatFood);
		EIC->BindAction(QuickDrinkWaterAction, ETriggerEvent::Started, this, &AOblivioCharacterController::OnQuickDrinkWater);
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
		if (ObjChar->IsAlive() && !ObjChar->IsOpeningLevelSequenceActive())
		{
			UpdateMouseRotation(DeltaTime);
		}
	}
}

void AOblivioCharacterController::UpdateMouseRotation(float DeltaTime)
{
	if (AOblivioCharacter* ObjChar = Cast<AOblivioCharacter>(GetPawn()))
	{
		FRotator TargetRotForced;
		if (ObjChar->TryConsumeForcedWorldLookRotation(TargetRotForced))
		{
			TargetRotForced.Pitch = 0.f;
			TargetRotForced.Roll = 0.f;

			FRotator const CurrentRot = ObjChar->GetActorRotation();
			float const RotationSpeed = ObjChar->GetForcedLookInterpSpeed();

			FRotator const SmoothRot = FMath::RInterpTo(CurrentRot, TargetRotForced, DeltaTime, RotationSpeed);
			ObjChar->SetActorRotation(SmoothRot);
			return;
		}
	}

	if (APawn* MyPawn = GetPawn())
	{
		FVector WorldLocation, WorldDirection;
		//마우스 커서의 위치를 월드 레이(Ray) 데이터로 변환.
		if (DeprojectMousePositionToWorld(WorldLocation, WorldDirection))
		{
			FVector PawnLoc = MyPawn->GetActorLocation();

			//조준선 방향의 Z축이 0이 아닐 때만 계산 (분모가 0이 되는 버그 방지)
			if (!FMath::IsNearlyZero(WorldDirection.Z))
			{
				//캐릭터 Z축 높이를 기준으로 평면과의 교차 거리(T) 및 가상 충돌 지점 계산
				float T = (PawnLoc.Z - WorldLocation.Z) / WorldDirection.Z;
				FVector VirtualImpactPoint = WorldLocation + (WorldDirection * T);

				//목표 회전값
				FRotator TargetRot = UKismetMathLibrary::FindLookAtRotation(PawnLoc, VirtualImpactPoint);
				TargetRot.Pitch = 0.f; //위아래로 기울지 않도록 고정
				TargetRot.Roll = 0.f;

				//현재 회전값
				FRotator CurrentRot = MyPawn->GetActorRotation();

				//회전 속도
				float RotationSpeed = 20.0f;
				float SensitivityMultiplier = 1.0f; //마우스 감도 기본 배율

				if (UOblivioGameUserSettings* Settings = UOblivioGameUserSettings::GetOblivioGameUserSettings())
				{
					SensitivityMultiplier = Settings->GetMouseSensitivity();
				}

				float FinalRotationSpeed = RotationSpeed * SensitivityMultiplier;

				FRotator SmoothRot = FMath::RInterpTo(CurrentRot, TargetRot, DeltaTime, FinalRotationSpeed);

				MyPawn->SetActorRotation(SmoothRot);
			}
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

void AOblivioCharacterController::OnCrouchStarted(const FInputActionValue& Value)
{
	if (AOblivioCharacter* ObjChar = Cast<AOblivioCharacter>(GetPawn()))
		ObjChar->StartCrouching();
}

void AOblivioCharacterController::OnCrouchCompleted(const FInputActionValue& Value)
{
	if (AOblivioCharacter* ObjChar = Cast<AOblivioCharacter>(GetPawn()))
		ObjChar->StopCrouching();
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
			ObjChar->UseFlashbang();
}
void AOblivioCharacterController::OnThrowBottle(const FInputActionValue& Value)
{
	if (AOblivioCharacter* ObjChar = Cast<AOblivioCharacter>(GetPawn()))
		if (IsValid(ObjChar->BottleClass))
			ObjChar->ThrowBottle();
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
void AOblivioCharacterController::OnQuickEatFood(const FInputActionValue& Value)
{
	if (AOblivioCharacter* ObjChar = Cast<AOblivioCharacter>(GetPawn()))
	{
		if (auto* InvComp = ObjChar->FindComponentByClass<UOblivioInventoryComponent>())
		{
			InvComp->ConsumeItemByQuickSlot(EItemType::Food);
		}
	}
}

void AOblivioCharacterController::OnQuickDrinkWater(const FInputActionValue& Value)
{
	if (AOblivioCharacter* ObjChar = Cast<AOblivioCharacter>(GetPawn()))
	{
		if (auto* InvComp = ObjChar->FindComponentByClass<UOblivioInventoryComponent>())
		{
			InvComp->ConsumeItemByQuickSlot(EItemType::Water);
		}
	}
}
void AOblivioCharacterController::OnSelectObstacle(const FInputActionValue& Value)
{
	// GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::White, TEXT("Number Key Pressed"));

	float Magnitude = Value.Get<float>();
	int32 SelectedIndex = FMath::RoundToInt(Magnitude);

	if (SelectedIndex <= 0)
	{
		SelectedIndex = 1;
	}
	if (AOblivioCharacter* ObjChar = Cast<AOblivioCharacter>(GetPawn()))
	{
		// 오직 크래프팅 창이 열려 있을 때만 작동하도록 제한
		if (ObjChar->bIsCraftingOpen && ObjChar->CraftingComponent)
		{
			if (CurrentSelectedIndex == SelectedIndex) return;
			CurrentSelectedIndex = SelectedIndex;

			if (auto* CraftingComp = ObjChar->FindComponentByClass<UOblivioCrafting>())
			{
				CraftingComp->SelectObstacle(SelectedIndex);
				CurrentSelectedIndex = 0;
			}
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
			ObjChar->PlaceObstacle();
		}
	}
}
void AOblivioCharacterController::OnInteract(const FInputActionValue& Value)
{
	if (AOblivioCharacter* ObjChar = Cast<AOblivioCharacter>(GetPawn()))
	{
		if (ObjChar->IsOpeningLevelSequenceActive())
		{
			return;
		}

		ObjChar->Interact();
	}
}
void AOblivioCharacterController::OnPauseToggle(const FInputActionValue& Value)
{
	if (AOblivioCharacter* ObjChar = Cast<AOblivioCharacter>(GetPawn()))
	{
		ObjChar->TogglePause();
	}
}