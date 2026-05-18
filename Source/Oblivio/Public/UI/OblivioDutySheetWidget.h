#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OblivioDutySheetWidget.generated.h"

/**
 * 당직표 UI — E키로 뷰포트 제거·게임 입력 복구.
 */
UCLASS()
class OBLIVIO_API UOblivioDutySheetWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UOblivioDutySheetWidget(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "Duty|Input")
	void HandleStashKeyPressed();

protected:
	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Duty|Input")
	bool bRemoveFromViewportAndRestoreGameInputOnStashKey = true;
};
