#include "UI/OblivioCabinetMashWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Widget.h"

UOblivioCabinetMashWidget::UOblivioCabinetMashWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bHasScriptImplementedTick = true;
	bIsFocusable = false;

	MashPromptMessage = NSLOCTEXT("CabinetMash", "Prompt",
		"E키를 연타하여 탈출!");
}

void UOblivioCabinetMashWidget::ResolveWidgetBindings()
{
	if (!MashPanel)
	{
		MashPanel = GetWidgetFromName(TEXT("MashPanel"));
	}

	if (!MashContentRoot)
	{
		MashContentRoot = GetWidgetFromName(TEXT("MashContentRoot"));
	}

	if (!MashProgressOverlay)
	{
		MashProgressOverlay = GetWidgetFromName(TEXT("MashProgressOverlay"));
	}

	if (!MashProgressBar)
	{
		MashProgressBar = Cast<UProgressBar>(GetWidgetFromName(TEXT("MashProgressBar")));
		if (!MashProgressBar)
		{
			MashProgressBar = Cast<UProgressBar>(GetWidgetFromName(TEXT("ProgressBar")));
		}
	}

	if (!ProgressText)
	{
		ProgressText = Cast<UTextBlock>(GetWidgetFromName(TEXT("ProgressText")));
		if (!ProgressText)
		{
			ProgressText = Cast<UTextBlock>(GetWidgetFromName(TEXT("CountText")));
		}
	}

	if (!PromptText)
	{
		PromptText = Cast<UTextBlock>(GetWidgetFromName(TEXT("PromptText")));
		if (!PromptText)
		{
			PromptText = Cast<UTextBlock>(GetWidgetFromName(TEXT("MessageText")));
		}
	}

	if (!PromptKeyImage)
	{
		PromptKeyImage = Cast<UImage>(GetWidgetFromName(TEXT("PromptKeyImage")));
	}
}

void UOblivioCabinetMashWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ResolveWidgetBindings();
	ResetSlideVisual();
	ResetPressFeedbackVisual();

	if (!bVisible)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		SetMashClusterVisibility(ESlateVisibility::Collapsed);
	}
	else
	{
		ApplyProgressVisuals();
		BeginSlideIn();
	}
}

void UOblivioCabinetMashWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateSlideAnimation(InDeltaTime);
	UpdatePressFeedback(InDeltaTime);
}

float UOblivioCabinetMashWidget::GetMashProgressRatio() const
{
	if (RequiredPressCount <= 0)
	{
		return 0.f;
	}

	return FMath::Clamp(
		static_cast<float>(CurrentPressCount) / static_cast<float>(RequiredPressCount),
		0.f,
		1.f);
}

UWidget* UOblivioCabinetMashWidget::GetSlideTargetWidget() const
{
	// E키 + Progress Bar를 한 덩어리로 슬라이드 (Overlay만 움직이면 PromptKeyImage와 따로 놀음)
	if (MashPanel)
	{
		return MashPanel;
	}

	if (MashContentRoot)
	{
		return MashContentRoot;
	}

	return GetRootWidget();
}

UWidget* UOblivioCabinetMashWidget::GetPressFeedbackTargetWidget() const
{
	if (PromptKeyImage)
	{
		return PromptKeyImage;
	}

	if (PromptText)
	{
		return PromptText;
	}

	return nullptr;
}

void UOblivioCabinetMashWidget::SetMashClusterVisibility(ESlateVisibility InVisibility)
{
	if (MashPanel || MashContentRoot)
	{
		if (MashPanel)
		{
			MashPanel->SetVisibility(InVisibility);
		}

		if (MashContentRoot)
		{
			MashContentRoot->SetVisibility(InVisibility);
		}

		return;
	}

	if (PromptKeyImage)
	{
		PromptKeyImage->SetVisibility(InVisibility);
	}

	if (MashProgressOverlay)
	{
		MashProgressOverlay->SetVisibility(InVisibility);
	}
	else if (MashProgressBar)
	{
		MashProgressBar->SetVisibility(InVisibility);
	}

	if (ProgressText)
	{
		ProgressText->SetVisibility(InVisibility);
	}
}

void UOblivioCabinetMashWidget::ApplyProgressVisuals()
{
	if (PromptKeyImage)
	{
		PromptKeyImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (PromptText)
		{
			PromptText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	else if (PromptText)
	{
		PromptText->SetVisibility(ESlateVisibility::HitTestInvisible);
		PromptText->SetText(MashPromptMessage);
	}

	if (ProgressText)
	{
		ProgressText->SetText(FText::Format(
			NSLOCTEXT("CabinetMash", "ProgressFormat", "{0} / {1}"),
			FText::AsNumber(CurrentPressCount),
			FText::AsNumber(FMath::Max(1, RequiredPressCount))));
	}

	const float Ratio = GetMashProgressRatio();
	if (MashProgressBar)
	{
		MashProgressBar->SetPercent(Ratio);
	}

	OnMashProgressUpdated(CurrentPressCount, RequiredPressCount, Ratio);
}

void UOblivioCabinetMashWidget::ApplySlideVisual(float NormalizedTime, bool bSlideIn)
{
	UWidget* const Target = GetSlideTargetWidget();
	if (!Target)
	{
		return;
	}

	const float Clamped = FMath::Clamp(NormalizedTime, 0.f, 1.f);
	const float Eased = bSlideIn
		? FMath::InterpEaseOut(0.f, 1.f, Clamped, 2.f)
		: FMath::InterpEaseIn(0.f, 1.f, Clamped, 2.f);

	const float TranslationY = bSlideIn
		? FMath::Lerp(SlideOffsetY, 0.f, Eased)
		: FMath::Lerp(0.f, SlideOffsetY, Eased);
	const float Opacity = bSlideIn ? Eased : (1.f - Eased);

	Target->SetRenderTranslation(FVector2D(0.f, TranslationY));
	Target->SetRenderOpacity(Opacity);
}

void UOblivioCabinetMashWidget::ResetSlideVisual()
{
	if (UWidget* const Target = GetSlideTargetWidget())
	{
		Target->SetRenderTranslation(FVector2D::ZeroVector);
		Target->SetRenderOpacity(1.f);
	}
}

void UOblivioCabinetMashWidget::ResetPressFeedbackVisual()
{
	if (UWidget* const Target = GetPressFeedbackTargetWidget())
	{
		Target->SetRenderScale(FVector2D(1.f, 1.f));
	}
}

void UOblivioCabinetMashWidget::BeginSlideIn()
{
	SlideState = ECabinetMashSlideState::SlideIn;
	SlideElapsed = 0.f;
	SetVisibility(ESlateVisibility::HitTestInvisible);
	SetMashClusterVisibility(ESlateVisibility::HitTestInvisible);
	ApplySlideVisual(0.f, true);
}

void UOblivioCabinetMashWidget::BeginSlideOut(bool bInstant)
{
	if (bInstant)
	{
		FinishSlideOut(true);
		return;
	}

	SlideState = ECabinetMashSlideState::SlideOut;
	SlideElapsed = 0.f;
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UOblivioCabinetMashWidget::UpdateSlideAnimation(float DeltaTime)
{
	if (SlideState == ECabinetMashSlideState::None)
	{
		return;
	}

	SlideElapsed += DeltaTime;

	if (SlideState == ECabinetMashSlideState::SlideIn)
	{
		const float Duration = FMath::Max(SlideInDuration, KINDA_SMALL_NUMBER);
		const float Alpha = SlideElapsed / Duration;
		ApplySlideVisual(Alpha, true);

		if (Alpha >= 1.f)
		{
			FinishSlideIn();
		}
		return;
	}

	if (SlideState == ECabinetMashSlideState::SlideOut)
	{
		const float Duration = FMath::Max(SlideOutDuration, KINDA_SMALL_NUMBER);
		const float Alpha = SlideElapsed / Duration;
		ApplySlideVisual(Alpha, false);

		if (Alpha >= 1.f)
		{
			FinishSlideOut(false);
		}
	}
}

void UOblivioCabinetMashWidget::FinishSlideIn()
{
	SlideState = ECabinetMashSlideState::None;
	SlideElapsed = 0.f;
	ResetSlideVisual();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	SetMashClusterVisibility(ESlateVisibility::HitTestInvisible);
}

void UOblivioCabinetMashWidget::FinishSlideOut(bool bInstant)
{
	SlideState = ECabinetMashSlideState::None;
	SlideElapsed = 0.f;
	bPressFeedbackActive = false;
	ResetSlideVisual();
	ResetPressFeedbackVisual();
	SetMashClusterVisibility(ESlateVisibility::Collapsed);
	SetVisibility(ESlateVisibility::Collapsed);
	bVisible = false;

	if (IsInViewport())
	{
		RemoveFromParent();
	}
}

void UOblivioCabinetMashWidget::UpdatePressFeedback(float DeltaTime)
{
	if (!bPressFeedbackActive)
	{
		return;
	}

	UWidget* const Target = GetPressFeedbackTargetWidget();
	if (!Target)
	{
		bPressFeedbackActive = false;
		return;
	}

	PressFeedbackElapsed += DeltaTime;
	const float Duration = FMath::Max(PressFeedbackDuration, KINDA_SMALL_NUMBER);
	const float Alpha = FMath::Clamp(PressFeedbackElapsed / Duration, 0.f, 1.f);
	const float Scale = FMath::Lerp(PressFeedbackScale, 1.f, Alpha);
	Target->SetRenderScale(FVector2D(Scale, Scale));

	if (Alpha >= 1.f)
	{
		bPressFeedbackActive = false;
		Target->SetRenderScale(FVector2D(1.f, 1.f));
	}
}

void UOblivioCabinetMashWidget::ShowMashPrompt(int32 InRequiredPressCount, bool bInstant)
{
	RequiredPressCount = FMath::Max(1, InRequiredPressCount);
	CurrentPressCount = 0;
	bVisible = true;
	ApplyProgressVisuals();

	if (bInstant)
	{
		SlideState = ECabinetMashSlideState::None;
		ResetSlideVisual();
		SetVisibility(ESlateVisibility::HitTestInvisible);
		SetMashClusterVisibility(ESlateVisibility::HitTestInvisible);
		return;
	}

	BeginSlideIn();
}

void UOblivioCabinetMashWidget::HideMashPrompt(bool bInstant)
{
	if (!bVisible && SlideState == ECabinetMashSlideState::None)
	{
		return;
	}

	BeginSlideOut(bInstant);
}

void UOblivioCabinetMashWidget::SetMashProgress(int32 InCurrentPressCount, int32 InRequiredPressCount)
{
	CurrentPressCount = FMath::Max(0, InCurrentPressCount);
	RequiredPressCount = FMath::Max(1, InRequiredPressCount);
	ApplyProgressVisuals();
}

void UOblivioCabinetMashWidget::PlayMashPressFeedback()
{
	UWidget* const Target = GetPressFeedbackTargetWidget();
	if (!Target)
	{
		return;
	}

	bPressFeedbackActive = true;
	PressFeedbackElapsed = 0.f;
	Target->SetRenderScale(FVector2D(PressFeedbackScale, PressFeedbackScale));
}
