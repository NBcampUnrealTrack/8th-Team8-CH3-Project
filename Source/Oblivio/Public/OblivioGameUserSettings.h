#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "OblivioGameUserSettings.generated.h"

UCLASS()
class OBLIVIO_API UOblivioGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	UOblivioGameUserSettings();

	// 블루프린트에서 쉽게 이 클래스를 불러오기 위한 함수
	UFUNCTION(BlueprintCallable, Category = "Settings")
	static UOblivioGameUserSettings* GetOblivioGameUserSettings();

	// 마우스 감도 Get/Set
	UFUNCTION(BlueprintPure, Category = "Settings")
	float GetMouseSensitivity() const;

	UFUNCTION(BlueprintCallable, Category = "Settings")
	void SetMouseSensitivity(float NewSensitivity);

	// 마스터 볼륨 Get/Set
	UFUNCTION(BlueprintPure, Category = "Settings")
	float GetMasterVolume() const;

	UFUNCTION(BlueprintCallable, Category = "Settings")
	void SetMasterVolume(float NewVolume);

protected:
	UPROPERTY(Config, BlueprintReadWrite, Category = "Settings")
	float MouseSensitivity;

	UPROPERTY(Config, BlueprintReadWrite, Category = "Settings")
	float MasterVolume;
};