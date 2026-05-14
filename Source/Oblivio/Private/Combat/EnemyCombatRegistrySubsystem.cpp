#include "Combat/EnemyCombatRegistrySubsystem.h"
#include "Engine/World.h"

void UEnemyCombatRegistrySubsystem::Deinitialize()
{
	RegisteredEnemies.Empty();
	Super::Deinitialize();
}

UEnemyCombatRegistrySubsystem* UEnemyCombatRegistrySubsystem::GetEnemyCombatRegistry(const UObject* WorldContextObject)
{
	if (!IsValid(WorldContextObject))
	{
		return nullptr;
	}
	UWorld* World = WorldContextObject->GetWorld();
	return World ? World->GetSubsystem<UEnemyCombatRegistrySubsystem>() : nullptr;
}

void UEnemyCombatRegistrySubsystem::CompactStaleEntries()
{
	RegisteredEnemies.RemoveAll([](const TWeakObjectPtr<AEnemyBase>& W) { return !W.IsValid(); });
}

void UEnemyCombatRegistrySubsystem::RegisterEnemy(AEnemyBase* Enemy)
{
	if (!IsValid(Enemy))
	{
		return;
	}

	CompactStaleEntries();

	for (const TWeakObjectPtr<AEnemyBase>& W : RegisteredEnemies)
	{
		if (W.Get() == Enemy)
		{
			return;
		}
	}

	RegisteredEnemies.Add(Enemy);
	OnEnemyRegistered.Broadcast(Enemy);
}

void UEnemyCombatRegistrySubsystem::UnregisterEnemy(AEnemyBase* Enemy)
{
	if (!Enemy)
	{
		return;
	}

	RegisteredEnemies.RemoveAll([Enemy](const TWeakObjectPtr<AEnemyBase>& W) { return W.Get() == Enemy; });
	OnEnemyUnregistered.Broadcast(Enemy);
}

void UEnemyCombatRegistrySubsystem::GetAllRegisteredEnemies(TArray<AEnemyBase*>& OutEnemies)
{
	CompactStaleEntries();
	OutEnemies.Reset();
	for (const TWeakObjectPtr<AEnemyBase>& W : RegisteredEnemies)
	{
		if (AEnemyBase* E = W.Get())
		{
			OutEnemies.Add(E);
		}
	}
}

void UEnemyCombatRegistrySubsystem::GetLivingEnemies(TArray<AEnemyBase*>& OutEnemies)
{
	GetAllRegisteredEnemies(OutEnemies);
	for (int32 i = OutEnemies.Num() - 1; i >= 0; --i)
	{
		AEnemyBase* E = OutEnemies[i];
		if (!IsValid(E) || !E->IsAlive())
		{
			OutEnemies.RemoveAtSwap(i);
		}
	}
}

void UEnemyCombatRegistrySubsystem::GatherAllEnemyCombatStates(TArray<FOblivioEnemyCombatSnapshot>& OutSnapshots)
{
	CompactStaleEntries();
	OutSnapshots.Reset();
	for (const TWeakObjectPtr<AEnemyBase>& W : RegisteredEnemies)
	{
		AEnemyBase* const E = W.Get();
		if (!IsValid(E))
		{
			continue;
		}

		FOblivioEnemyCombatSnapshot S;
		S.Enemy = E;
		S.AIState = E->GetEnemyState();
		S.CCState = E->GetCrowdControlState();
		S.CurrentHealth = E->GetCurrentHealthForUI();
		S.MaxHealth = E->GetMaxHealthForUI();
		S.bIsAlive = E->IsAlive();
		OutSnapshots.Add(S);
	}
}

void UEnemyCombatRegistrySubsystem::NotifyEnemyDamaged(AEnemyBase* Enemy, float DamageAmount, float CurrentHealth, float MaxHealth)
{
	OnAnyEnemyDamaged.Broadcast(Enemy, DamageAmount, CurrentHealth, MaxHealth);
}

void UEnemyCombatRegistrySubsystem::NotifyEnemyDied(AEnemyBase* Enemy)
{
	OnAnyEnemyDied.Broadcast(Enemy);
}

void UEnemyCombatRegistrySubsystem::NotifyEnemyAttackCommitted(AEnemyBase* Enemy, AActor* Target, float DamageAmount,
	UClass* DamageTypeClass)
{
	OnAnyEnemyAttackCommitted.Broadcast(Enemy, Target, DamageAmount, DamageTypeClass);
}

void UEnemyCombatRegistrySubsystem::NotifyEnemyFSMStateChanged(AEnemyBase* Enemy, EEnemyAIState OldState, EEnemyAIState NewState)
{
	OnAnyEnemyFSMStateChanged.Broadcast(Enemy, OldState, NewState);
}

int32 UEnemyCombatRegistrySubsystem::GetRegisteredEnemyCount()
{
	CompactStaleEntries();

	int32 Count = 0;
	for (const TWeakObjectPtr<AEnemyBase>& W : RegisteredEnemies)
	{
		if (W.IsValid())
		{
			++Count;
		}
	}
	return Count;
}
