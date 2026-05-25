#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OblivioCabinetMashWidget.generated.h"

class UProgressBar;
class UTextBlock;
class UImage;
class UWidget;

UENUM()
enum class ECabinetMashSlideState : uint8
{
	None,
	SlideIn,
	SlideOut
};

/**
 * 캐비넷 에너미 E 연타 탈출 QTE UI.
 *
 * WBP 권장 구조 (PromptKeyImage + Progress Bar 반드시 같은 부모 안):
 *   MashPanel (또는 MashContentRoot) ← 슬라이드/페이드/펀치 대상
 *     PromptKeyImage
 *     MashProgressOverlay
 *       MashProgressBar
 *       ProgressText
 */
UCLASS(Abstract, Blueprintable)
class OBLIVIO_API UOblivioCabinetMashWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UOblivioCabinetMashWidget(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "Cabinet|Mash")
	void ShowMashPrompt(int32 RequiredPressCount, bool bInstant = false);

	UFUNCTION(BlueprintCallable, Category = "Cabinet|Mash")
	void HideMashPrompt(bool bInstant = false);

	UFUNCTION(BlueprintCallable, Category = "Cabinet|Mash")
	void SetMashProgress(int32 CurrentPressCount, int32 RequiredPressCount);

	/** E 입력마다 C++에서 호출 — BP에서 펀치 VFX/사운드 연결 가능. */
	UFUNCTION(BlueprintCallable, Category = "Cabinet|Mash")
	void PlayMashPressFeedback();

	UFUNCTION(BlueprintPure, Category = "Cabinet|Mash")
	int32 GetCurrentPressCount() const { return CurrentPressCount; }

	UFUNCTION(BlueprintPure, Category = "Cabinet|Mash")
	int32 GetRequiredPressCount() const { return RequiredPressCount; }

	UFUNCTION(BlueprintPure, Category = "Cabinet|Mash")
	float GetMashProgressRatio() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Cabinet|Mash")
	void OnMashProgressUpdated(int32 Current, int32 Required, float ProgressRatio);

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> MashPanel;

	/** MashPanel 대신 쓸 때 — PromptKeyImage·Progress Bar를 이 안에 함께 넣기. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> MashContentRoot;

	/** MashProgressBar + ProgressText Overlay. 슬라이드 대상 아님(MashPanel이 대상). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> MashProgressOverlay;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PromptText;

	/** E 키 박스 이미지 (PromptText 대체). WBP Image 이름: PromptKeyImage */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> PromptKeyImage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ProgressText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> MashProgressBar;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cabinet|Mash")
	FText MashPromptMessage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cabinet|Mash|Animation")
	float SlideInDuration = 0.3f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cabinet|Mash|Animation")
	float SlideOutDuration = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cabinet|Mash|Animation")
	float SlideOffsetY = 120.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cabinet|Mash|Animation")
	float PressFeedbackDuration = 0.12f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cabinet|Mash|Animation")
	float PressFeedbackScale = 1.08f;

	UPROPERTY(BlueprintReadOnly, Category = "Cabinet|Mash")
	int32 CurrentPressCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Cabinet|Mash")
	int32 RequiredPressCount = 0;

private:
	UWidget* GetSlideTargetWidget() const;
	UWidget* GetPressFeedbackTargetWidget() const;
	void ResolveWidgetBindings();
	void SetMashClusterVisibility(ESlateVisibility InVisibility);
	void ApplyProgressVisuals();
	void BeginSlideIn();
	void BeginSlideOut(bool bInstant);
	void UpdateSlideAnimation(float DeltaTime);
	void ApplySlideVisual(float NormalizedTime, bool bSlideIn);
	void ResetSlideVisual();
	void ResetPressFeedbackVisual();
	void FinishSlideIn();
	void FinishSlideOut(bool bInstant);
	void UpdatePressFeedback(float DeltaTime);

	bool bVisible = false;
	ECabinetMashSlideState SlideState = ECabinetMashSlideState::None;
	float SlideElapsed = 0.f;
	float PressFeedbackElapsed = 0.f;
	bool bPressFeedbackActive = false;
};
