#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TankMembraneSpawnPoint.generated.h"

class USceneComponent;
class UBillboardComponent;
class UArrowComponent;

/**
 * 레벨에 배치 → BeginPlay 에 UTankMembraneSpawnSubsystem 자동 등록.
 * Tank 양막 패턴(Membrane)이 트리거되면 등록된 지점들 중 무작위 2개에서 양막이 스폰된다.
 * 최소 2개 이상 배치 필요. 에디터 가시성을 위해 Billboard + Arrow 컴포넌트가 붙어 있다.
 */
UCLASS(Blueprintable)
class OBLIVIO_API ATankMembraneSpawnPoint : public AActor
{
	GENERATED_BODY()

public:
	ATankMembraneSpawnPoint();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category = "Tank|Membrane|Spawn")
	FTransform GetSpawnTransform() const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Membrane|Spawn")
	TObjectPtr<USceneComponent> RootScene;

#if WITH_EDITORONLY_DATA
	/** 에디터 뷰포트에서 위치 확인용 빌보드. */
	UPROPERTY()
	TObjectPtr<UBillboardComponent> EditorBillboard;

	/** 에디터 뷰포트에서 정면 방향(양막 부채꼴 기준 +X) 확인용 화살표. */
	UPROPERTY()
	TObjectPtr<UArrowComponent> EditorArrow;
#endif
};
