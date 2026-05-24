#include "UI/OblivioFlashlightPromptWidget.h"

#include "Components/TextBlock.h"
#include "Components/Widget.h"

UOblivioFlashlightPromptWidget::UOblivioFlashlightPromptWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bHasScriptImplementedTick = true;

	AcquireWithEMessage = NSLOCTEXT("FlashlightPrompt", "AcquireWithE",
		"E키를 눌러 손전등을 획득하세요.");
	TurnOnWithFMessage = NSLOCTEXT("FlashlightPrompt", "TurnOnWithF",
		"F키를 눌러 손전등을 키고 적을 처치하세요.");
}

void UOblivioFlashlightPromptWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ResetSlideVisual();

	if (CurrentPhase == EFlashlightPromptPhase::Hidden)
	{
		SetVisibility(ESlateVisibility::Collapsed);
	}
	else
	{
		ApplyPromptText(CurrentPhase);
		BeginSlideIn();
	}
}

void UOblivioFlashlightPromptWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateSlideAnimation(InDeltaTime);
}

UWidget* UOblivioFlashlightPromptWidget::GetSlideTargetWidget() const
{
	if (ToastPanel)
	{
		return ToastPanel;
	}

	return GetRootWidget();
}

void UOblivioFlashlightPromptWidget::ApplyPromptText(EFlashlightPromptPhase Phase)
{
	if (MessageText)
	{
		MessageText->SetText(GetMessageForPhase(Phase));
	}
}

FText UOblivioFlashlightPromptWidget::GetMessageForPhase(EFlashlightPromptPhase Phase) const
{
	switch (Phase)
	{
	case EFlashlightPromptPhase::AcquireWithE:
		return AcquireWithEMessage;
	case EFlashlightPromptPhase::TurnOnWithF:
		return TurnOnWithFMessage;
	default:
		return FText::GetEmpty();
	}
}

void UOblivioFlashlightPromptWidget::ApplySlideVisual(float NormalizedTime, bool bSlideIn)
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

	const float TranslationX = bSlideIn
		? FMath::Lerp(-SlideOffsetX, 0.f, Eased)
		: FMath::Lerp(0.f, -SlideOffsetX, Eased);
	const float Opacity = bSlideIn ? Eased : (1.f - Eased);

	Target->SetRenderTranslation(FVector2D(TranslationX, 0.f));
	Target->SetRenderOpacity(Opacity);
}

void UOblivioFlashlightPromptWidget::ResetSlideVisual()
{
	if (UWidget* const Target = GetSlideTargetWidget())
	{
		Target->SetRenderTranslation(FVector2D::ZeroVector);
		Target->SetRenderOpacity(1.f);
	}
}

void UOblivioFlashlightPromptWidget::BeginSlideIn()
{
	SlideState = EFlashlightPromptSlideState::SlideIn;
	SlideElapsed = 0.f;
	SetVisibility(ESlateVisibility::HitTestInvisible);
	ApplySlideVisual(0.f, true);
}

void UOblivioFlashlightPromptWidget::BeginSlideOut(bool bInstant)
{
	if (bInstant)
	{
		FinishSlideOut(true);
		return;
	}

	SlideState = EFlashlightPromptSlideState::SlideOut;
	SlideElapsed = 0.f;
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UOblivioFlashlightPromptWidget::UpdateSlideAnimation(float DeltaTime)
{
	if (SlideState == EFlashlightPromptSlideState::None)
	{
		return;
	}

	SlideElapsed += DeltaTime;

	if (SlideState == EFlashlightPromptSlideState::SlideIn)
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

	if (SlideState == EFlashlightPromptSlideState::SlideOut)
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

void UOblivioFlashlightPromptWidget::FinishSlideIn()
{
	SlideState = EFlashlightPromptSlideState::None;
	SlideElapsed = 0.f;
	ResetSlideVisual();
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UOblivioFlashlightPromptWidget::FinishSlideOut(bool bInstant)
{
	SlideState = EFlashlightPromptSlideState::None;
	SlideElapsed = 0.f;
	ResetSlideVisual();
	SetVisibility(ESlateVisibility::Collapsed);

	if (IsInViewport())
	{
		RemoveFromParent();
	}
}

void UOblivioFlashlightPromptWidget::SetPromptPhase(EFlashlightPromptPhase NewPhase, bool bInstant)
{
	if (NewPhase == EFlashlightPromptPhase::Hidden)
	{
		if (CurrentPhase == EFlashlightPromptPhase::Hidden && SlideState == EFlashlightPromptSlideState::None)
		{
			return;
		}

		CurrentPhase = EFlashlightPromptPhase::Hidden;
		BeginSlideOut(bInstant);
		return;
	}

	const bool bPhaseChanged = CurrentPhase != NewPhase;
	CurrentPhase = NewPhase;
	ApplyPromptText(NewPhase);

	if (bInstant)
	{
		SlideState = EFlashlightPromptSlideState::None;
		ResetSlideVisual();
		SetVisibility(ESlateVisibility::HitTestInvisible);
		return;
	}

	if (bPhaseChanged || SlideState == EFlashlightPromptSlideState::None)
	{
		BeginSlideIn();
	}
}
