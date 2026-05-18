#include "OblivioGameUserSettings.h"

UOblivioGameUserSettings::UOblivioGameUserSettings()
{
	// 최초 실행 시 적용될 기본값 세팅
	MouseSensitivity = 0.5f;
	MasterVolume = 0.5f;
}

UOblivioGameUserSettings* UOblivioGameUserSettings::GetOblivioGameUserSettings()
{
	// 전역 GameUserSettings를 우리의 커스텀 클래스로 캐스팅해서 반환
	return Cast<UOblivioGameUserSettings>(GEngine->GetGameUserSettings());
}

float UOblivioGameUserSettings::GetMouseSensitivity() const { return MouseSensitivity; }
void UOblivioGameUserSettings::SetMouseSensitivity(float NewSensitivity) { MouseSensitivity = NewSensitivity; }

float UOblivioGameUserSettings::GetMasterVolume() const { return MasterVolume; }
void UOblivioGameUserSettings::SetMasterVolume(float NewVolume) { MasterVolume = NewVolume; }