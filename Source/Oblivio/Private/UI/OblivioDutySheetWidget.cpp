#include "UI/OblivioDutySheetWidget.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

UOblivioDutySheetWidget::UOblivioDutySheetWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsFocusable = true;
}

void UOblivioDutySheetWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);

	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetKeyboardFocus(TakeWidget(), EFocusCause::SetDirectly);
	}
}

void UOblivioDutySheetWidget::HandleStashKeyPressed()
{
	APlayerController* const PC = GetOwningPlayer();

	if (bRemoveFromViewportAndRestoreGameInputOnStashKey && PC)
	{
		FInputModeGameOnly InputMode;
		PC->SetInputMode(InputMode);
		PC->SetShowMouseCursor(false);
		PC->SetIgnoreLookInput(false);
		PC->SetIgnoreMoveInput(false);
		RemoveFromParent();
	}
}

FReply UOblivioDutySheetWidget::NativeOnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::E)
	{
		HandleStashKeyPressed();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(MyGeometry, InKeyEvent);
}
