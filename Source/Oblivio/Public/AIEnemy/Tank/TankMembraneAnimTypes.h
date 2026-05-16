#pragma once

#include "CoreMinimal.h"
#include "TankMembraneAnimTypes.generated.h"

/** ATankMembraneEmitterActor — 블루프린트에서 한 단계(스폰)만 쓸 때용. */
UENUM(BlueprintType)
enum class ETankMembraneEmitterAnimPhase : uint8
{
	None UMETA(DisplayName = "없음"),
	/** 이펙터/스폰 연출 기본 분기. (구 등장·조준·발사·종료는 이 한 값으로 통합) */
	Spawn UMETA(DisplayName = "스폰"),
};
