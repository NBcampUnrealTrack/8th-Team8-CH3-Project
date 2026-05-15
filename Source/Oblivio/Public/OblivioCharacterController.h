#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "OblivioCharacterController.generated.h"

UCLASS()
class OBLIVIO_API AOblivioCharacterController : public APlayerController
{
	GENERATED_BODY()
	
public:
	AOblivioCharacterController();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

	// --- Enhanced Input 에셋 ---
	UPROPERTY(EditAnywhere, Category = "Input")
	class UInputMappingContext* DefaultMappingContext;

	UPROPERTY(EditAnywhere, Category = "Input")
	class UInputMappingContext* InventoryMappingContext; // UI용 컨텍스트

	UPROPERTY(EditAnywhere, Category = "Input")
	class UInputAction* MoveAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	class UInputAction* LookAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	class UInputAction* JumpAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	class UInputAction* WheelAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	class UInputAction* RunAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	class UInputAction* CrouchAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	class UInputAction* FlashlightToggleAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	class UInputAction* FlashbangAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	class UInputAction* FlareAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	class UInputAction* ReloadAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	class UInputAction* InventoryAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	class UInputAction* CraftingAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	class UInputAction* RotateAction;

	UPROPERTY(EditAnyWhere, Category = "Input")
	class UInputAction* PlaceObstacleAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	class UInputAction* InteractAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	class UInputAction* PauseAction;

	// --- 입력 처리 함수 ---
	void OnMove(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);
	void OnJumpStarted(const FInputActionValue& Value);
	void OnJumpCompleted(const FInputActionValue& Value);
	void OnWheel(const FInputActionValue& Value);
	void OnRunStarted(const FInputActionValue& Value);
	void OnRunCompleted(const FInputActionValue& Value);
	void OnCrouchStarted(const FInputActionValue& Value);
	void OnCrouchCompleted(const FInputActionValue& Value);
	void OnFlashlightToggle(const FInputActionValue& Value);
	void OnFlashbang(const FInputActionValue& Value);
	void OnFlare(const FInputActionValue& Value);
	void OnReload(const FInputActionValue& Value);
	void OnInventoryToggle(const FInputActionValue& Value);
	void OnCraftingToggle(const FInputActionValue& Value);
	void OnRotatePreview(const FInputActionValue& Vlaue);
	void OnPlaceObstacle(const FInputActionValue& Value);
	void OnInteract(const FInputActionValue& Value);
	void OnPauseToggle(const FInputActionValue& Value);

	void UpdateMouseRotation(float DeltaTime);

	//크래프팅 전용
	UPROPERTY(EditAnywhere, Category = "Input")
	class UInputMappingContext* CraftingMappingContext;

	UPROPERTY(EditAnywhere, Category = "Input")
	class UInputAction* SelectObstacleAction;

	void OnSelectObstacle(const FInputActionValue& Value);

private:
	bool bIsInventoryOpen = false;
	bool bIsCraftingOpen = false;

	int32 CurrentSelectedIndex = 0;
};
