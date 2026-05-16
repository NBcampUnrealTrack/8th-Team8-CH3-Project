#include "AIEnemy/Tank/TankMembraneSpawnSubsystem.h"
#include "AIEnemy/Tank/TankMembraneSpawnPoint.h"

void UTankMembraneSpawnSubsystem::RegisterPoint(ATankMembraneSpawnPoint* Point)
{
	if (!IsValid(Point))
	{
		return;
	}
	CompactInvalidEntries();
	Points.AddUnique(Point);
}

void UTankMembraneSpawnSubsystem::UnregisterPoint(ATankMembraneSpawnPoint* Point)
{
	if (!Point)
	{
		return;
	}
	CompactInvalidEntries();
	for (int32 I = Points.Num() - 1; I >= 0; --I)
	{
		if (Points[I].Get() == Point)
		{
			Points.RemoveAtSwap(I);
		}
	}
}

void UTankMembraneSpawnSubsystem::CompactInvalidEntries()
{
	for (int32 I = Points.Num() - 1; I >= 0; --I)
	{
		if (!Points[I].IsValid())
		{
			Points.RemoveAtSwap(I);
		}
	}
}

int32 UTankMembraneSpawnSubsystem::GetRegisteredPointCount() const
{
	int32 Count = 0;
	for (TWeakObjectPtr<ATankMembraneSpawnPoint> const& W : Points)
	{
		if (W.IsValid())
		{
			++Count;
		}
	}
	return Count;
}

bool UTankMembraneSpawnSubsystem::TryPickTwoRandomSpawnTransforms(FTransform& OutA, FTransform& OutB) const
{
	TArray<ATankMembraneSpawnPoint const*, TInlineAllocator<16>> Valid;
	Valid.Reserve(Points.Num());
	for (TWeakObjectPtr<ATankMembraneSpawnPoint> const& W : Points)
	{
		if (ATankMembraneSpawnPoint* const P = W.Get())
		{
			Valid.Add(P);
		}
	}
	const int32 N = Valid.Num();
	if (N < 2)
	{
		return false;
	}
	int32 IdxA = FMath::RandRange(0, N - 1);
	int32 IdxB = FMath::RandRange(0, N - 1);
	while (IdxB == IdxA)
	{
		IdxB = FMath::RandRange(0, N - 1);
	}
	OutA = Valid[IdxA]->GetSpawnTransform();
	OutB = Valid[IdxB]->GetSpawnTransform();
	return true;
}
