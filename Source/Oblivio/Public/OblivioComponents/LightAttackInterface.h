//LightAttackInterface.h

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "LightAttackInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class ULightAttackInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class OBLIVIO_API ILightAttackInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	/**
	 * 빛 공격 1회 트리거.
	 * @param SourceLocation  광원 시작 위치(월드)
	 * @param LightDirection  광원 방향(월드, 정규화 안 돼 있어도 됨)
	 * @param ExposureDt      이 호출 1회가 의미하는 "빛 노출 시간(초)". 누적 흡수/누적 경직(Luxeater/Scream)
	 *                        같은 누적형 기믹은 이 값을 노출량으로 사용한다.
	 *                        0 이면 호출자 측에서 노출량을 의미하지 않는 호출(데미지만 처리).
	 */
	virtual void CreateLightAttack(FVector SourceLocation, FVector LightDirection, float ExposureDt = 0.f) = 0;
};
