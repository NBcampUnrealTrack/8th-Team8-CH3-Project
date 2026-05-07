#pragma once

#include "CoreMinimal.h"
#include "Weapon/WeaponBase.h"
#include "Weapon/ThrowableWeapon.h"
#include "OblivioComponents/CombatInterface.h"

#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "OblivioCharacter.generated.h"

// 피격 판정용 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FPlayerDamagedSignature, float, DamageAmount, float, CurrentHealth, float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNearbyItemChanged, class AOblivioItemBase*, NearbyItem);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPlayerAnimationEvent);

class UOblivioCrafting;
class UOblivioInventoryComponent;
class USoundPropagationComponent;
class UPlayerCombatComponent;

UCLASS()
class OBLIVIO_API AOblivioCharacter : public ACharacter, public ICombatInterface
{
	GENERATED_BODY()

public:
	AOblivioCharacter();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

public:
	//===========================
	//Components (컴포넌트)
	//==========================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Camera")
	class USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Camera")
	class UCameraComponent* TopDownCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Light")
	class USpotLightComponent* FlashlightComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Light")
	class UPointLightComponent* FlashbangLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Crafting")
	TObjectPtr<UOblivioCrafting> CraftingComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Combat")
	TObjectPtr<USoundPropagationComponent> SoundPropagationComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Combat")
	TObjectPtr<UPlayerCombatComponent> CombatComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Inventory")
	TObjectPtr<UOblivioInventoryComponent> InventoryComponent;

	//===================================
	//Survival & Status (생존 및 상태)
	//==================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status|Health")
	float MaxHealth = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Status|Health")
	float CurrentHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status|Survival")
	float Hunger = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status|Survival")
	float Thirst = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status|Flashlight")
	float Battery = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status|Flashlight")
	float BatteryDepletionRate = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status|Flashlight")
	bool bIsFlashlightOn = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status|Upgrade")
	bool bCanAdjustFocus = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	int32 BatteryItemCount = 0;

	bool bIsDead = false;
	bool bIsRunning = false;
	bool bIsInventoryOpen = false;
	bool bIsCraftingOpen = false;
	bool bIsPauseOpen = false;
	float CurrentFocusAlpha = 0.5f;

	void UpdateStatus(float DeltaTime);
	void UpdateFlashlightVisuals();
	void ReloadBattery();

	UFUNCTION(BlueprintCallable, Category = "Status|Health")
	void HandleDeath();

	//=================================
	// Weapons & Items (무기 및 아이템)
	//==================================
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	TSubclassOf<AWeaponBase> FlashlightWeapon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	TSubclassOf<AThrowableWeapon> FlashbangWeapon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	TSubclassOf<AThrowableWeapon> FlareWeapon;

	TObjectPtr<AWeaponBase> CurrentWeapon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	float WheelControlMultiplier = 3.0f;

	FVector GetAimingLocation();
	void BeginThrow(TSubclassOf<AThrowableWeapon> Weapon);
	FPlayerAnimationEvent OnPlayerThrow;
	UFUNCTION()
	void ThrowWeapon();
	TSubclassOf<AThrowableWeapon> PendingThrowClass;
	bool bIsThrowing;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Animation")
	UAnimMontage* ThrowMontage;


	FTimerHandle FlashbangTimerHandle;
	float FlashbangIntensity = 0.0f;
	void FadeOutFlashbang();

	// =====================================
	// Movement & Interaction 이동 및 상호작용
	// =====================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float WalkSpeed = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float RunSpeed = 550.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	float InteractionDistance = 200.0f;

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnNearbyItemChanged OnNearbyItemChanged;

	void SetNearbyItem(class AOblivioItemBase* Item);
	// 컨트롤러 바인딩 함수들
	void Move(const FVector2D& Value);
	void StartRunning();
	void StopRunning();
	void ToggleFlashlight();
	//void UseFlashbang();
	//void UseFlare();
	void AdjustFocus(float Value);
	void ToggleInventory();
	void ToggleCrafting();
	void PlaceObstacle();
	void Interact();
	
	UFUNCTION(BlueprintCallable, Category = "UI")
	void TogglePause();
	
	UFUNCTION(BlueprintImplementableEvent, Category = "UI")
	void OnPauseToggle(bool bIsOpen);

	//===============================
	// Combat Interface (전투 시스템)
	//===============================
	virtual void ApplyHealth(float Damage) override;
	virtual void ApplyCCSlow(float SpeedMultiplier, float Duration) override;
	virtual void ApplyCCStun(float Duration) override;
	virtual bool IsAlive() const override;

	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FPlayerDamagedSignature OnPlayerDamaged;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound|Animation")
	TObjectPtr<USoundBase> FootstepSound;
	UFUNCTION(BlueprintCallable)
	void GenerateFootstep();
	FPlayerAnimationEvent OnPlayerFootstep;
	FTimerHandle FootstepTimerHandle;

private:
	UPROPERTY()
	class AOblivioItemBase* CurrentNearbyItem = nullptr;
};