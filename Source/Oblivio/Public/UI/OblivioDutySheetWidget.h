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

	/** E키 등에서 호출. 블루프린트에서 오버라이드해 디스패처 등 연출 후 Parent 로 닫기 처리. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Duty|Input")
	void HandleStashKeyPressed();
	virtual void HandleStashKeyPressed_Implementation();

protected:
	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Duty|Input")
	bool bRemoveFromViewportAndRestoreGameInputOnStashKey = true;
};
