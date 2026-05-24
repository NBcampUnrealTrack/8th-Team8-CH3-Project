#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OblivioFlashlightPromptWidget.generated.h"

class UTextBlock;
class UWidget;

UENUM(BlueprintType)
enum class EFlashlightPromptPhase : uint8
{
	Hidden,
	/** E키로 월드 손전등 획득 안내 */
	AcquireWithE,
	/** F키로 손전등 켜기·전투 안내 */
	TurnOnWithF
};

UENUM()
enum class EFlashlightPromptSlideState : uint8
{
	None,
	SlideIn,
	SlideOut
};

/**
 * 손전등 튜토리얼용 토스트 UI. 슬라이드 인/아웃은 C++에서 처리.
 * WBP 자식: ToastPanel(선택) + MessageText.
 */
UCLASS(Abstract, Blueprintable)
class OBLIVIO_API UOblivioFlashlightPromptWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UOblivioFlashlightPromptWidget(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "Flashlight|Prompt")
	void SetPromptPhase(EFlashlightPromptPhase NewPhase, bool bInstant = false);

	UFUNCTION(BlueprintPure, Category = "Flashlight|Prompt")
	EFlashlightPromptPhase GetPromptPhase() const { return CurrentPhase; }

	UFUNCTION(BlueprintPure, Category = "Flashlight|Prompt")
	FText GetMessageForPhase(EFlashlightPromptPhase Phase) const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ToastPanel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MessageText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flashlight|Prompt|Animation")
	float SlideInDuration = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flashlight|Prompt|Animation")
	float SlideOutDuration = 0.3f;

	/** 왼쪽에서 들어오는 슬라이드 거리(px). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flashlight|Prompt|Animation")
	float SlideOffsetX = 200.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flashlight|Prompt")
	FText AcquireWithEMessage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flashlight|Prompt")
	FText TurnOnWithFMessage;

	UPROPERTY(BlueprintReadOnly, Category = "Flashlight|Prompt")
	EFlashlightPromptPhase CurrentPhase = EFlashlightPromptPhase::Hidden;

private:
	UWidget* GetSlideTargetWidget() const;
	void ApplyPromptText(EFlashlightPromptPhase Phase);
	void BeginSlideIn();
	void BeginSlideOut(bool bInstant);
	void UpdateSlideAnimation(float DeltaTime);
	void ApplySlideVisual(float NormalizedTime, bool bSlideIn);
	void ResetSlideVisual();
	void FinishSlideIn();
	void FinishSlideOut(bool bInstant);

	EFlashlightPromptSlideState SlideState = EFlashlightPromptSlideState::None;
	float SlideElapsed = 0.f;
};
