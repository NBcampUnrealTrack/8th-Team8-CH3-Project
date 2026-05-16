#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TankMembraneSpawnSubsystem.generated.h"

class ATankMembraneSpawnPoint;

/** 레벨의 ATankMembraneSpawnPoint 수집 — 서버에서 랜덤 2지점 선택용. */
UCLASS()
class OBLIVIO_API UTankMembraneSpawnSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void RegisterPoint(ATankMembraneSpawnPoint* Point);
	void UnregisterPoint(ATankMembraneSpawnPoint* Point);

	UFUNCTION(BlueprintPure, Category = "Tank|Membrane|Spawn")
	int32 GetRegisteredPointCount() const;

	UFUNCTION(BlueprintCallable, Category = "Tank|Membrane|Spawn")
	bool TryPickTwoRandomSpawnTransforms(FTransform& OutA, FTransform& OutB) const;

private:
	TArray<TWeakObjectPtr<ATankMembraneSpawnPoint>> Points;

	void CompactInvalidEntries();
};
