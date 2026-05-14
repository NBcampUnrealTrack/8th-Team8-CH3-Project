#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/DamageType.h"
#include "Templates/SubclassOf.h"
#include "AIEnemy/EnemyBase.h"

#include "EnemyCombatRegistrySubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEnemyCombatRegistryEnemyDelegate, AEnemyBase*, Enemy);
/** (Enemy, DamageAmount, CurrentHealth, MaxHealth) — TakeDamage 차감 직후. Heal/Set은 부호로 구분. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FEnemyCombatRegistryDamageDelegate, AEnemyBase*, Enemy, float, DamageAmount, float, CurrentHealth, float, MaxHealth);
/** (Enemy, Target, DamageAmount, DamageTypeClass) — nullptr 타입이면 기본 UDamageType. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FEnemyCombatRegistryAttackDelegate, AEnemyBase*, Enemy, AActor*, Target, float, DamageAmount, UClass*, DamageTypeClass);
/** (Enemy, Old, New) — FSM 전이. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FEnemyCombatRegistryFSMDelegate, AEnemyBase*, Enemy, EEnemyAIState, OldState, EEnemyAIState, NewState);

/** 전투/시나리오 BP에서 레지스트리에 바인드해 스폰·제거 알림 받기 */
USTRUCT(BlueprintType)
struct FOblivioEnemyCombatSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Combat")
	TObjectPtr<AEnemyBase> Enemy = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Combat")
	EEnemyAIState AIState = EEnemyAIState::Idle;

	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Combat")
	EEnemyCCState CCState = EEnemyCCState::None;

	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Combat")
	float CurrentHealth = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Combat")
	float MaxHealth = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Combat")
	bool bIsAlive = false;
};

/** 현재 월드의 모든 AEnemyBase를 추적한다. 전투 시스템에서 일괄 조회·스냅샷에 사용. */
UCLASS()
class OBLIVIO_API UEnemyCombatRegistrySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	UPROPERTY(BlueprintAssignable, Category = "Enemy|Combat")
	FEnemyCombatRegistryEnemyDelegate OnEnemyRegistered;

	UPROPERTY(BlueprintAssignable, Category = "Enemy|Combat")
	FEnemyCombatRegistryEnemyDelegate OnEnemyUnregistered;

	/** 등록된 어떤 적이든 사망 시. 매 적에 일일이 바인드하지 않고 한 번에 받을 수 있다. */
	UPROPERTY(BlueprintAssignable, Category = "Enemy|Combat")
	FEnemyCombatRegistryEnemyDelegate OnAnyEnemyDied;

	UPROPERTY(BlueprintAssignable, Category = "Enemy|Combat")
	FEnemyCombatRegistryDamageDelegate OnAnyEnemyDamaged;

	UPROPERTY(BlueprintAssignable, Category = "Enemy|Combat")
	FEnemyCombatRegistryAttackDelegate OnAnyEnemyAttackCommitted;

	UPROPERTY(BlueprintAssignable, Category = "Enemy|Combat")
	FEnemyCombatRegistryFSMDelegate OnAnyEnemyFSMStateChanged;

	void RegisterEnemy(AEnemyBase* Enemy);
	void UnregisterEnemy(AEnemyBase* Enemy);

	UFUNCTION(BlueprintCallable, Category = "Enemy|Combat", meta = (WorldContext = "WorldContextObject"))
	static UEnemyCombatRegistrySubsystem* GetEnemyCombatRegistry(const UObject* WorldContextObject);

	/** 유효 참조만 모은다(자동으로 잘못된 Weak 제거). */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Combat")
	void GetAllRegisteredEnemies(TArray<AEnemyBase*>& OutEnemies);

	UFUNCTION(BlueprintCallable, Category = "Enemy|Combat")
	void GetLivingEnemies(TArray<AEnemyBase*>& OutEnemies);

	/** 등록된 적 전체의 FSM·CC·체력 등 한 번에 채운다 */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Combat")
	void GatherAllEnemyCombatStates(TArray<FOblivioEnemyCombatSnapshot>& OutSnapshots);

	UFUNCTION(BlueprintCallable, Category = "Enemy|Combat")
	int32 GetRegisteredEnemyCount();

	// C++ 전용 템플릿 API — UFUNCTION이 아니므로 UHT와 무관.
	// 전투 시스템에서 한 줄로 모든 적, 또는 특정 타입(보스 등)에 일괄 작업 수행.
	//   ForEachEnemy<T>(Pred)         : 등록된 모든 적 중 T로 캐스팅 가능한 것에 대해 Pred(T*) 실행.
	//   ForEachLivingEnemy<T>(Pred)   : 위와 같지만 IsAlive() 통과한 것만.
	//   GetEnemiesOfType<T>(Out)      : 등록된 적 중 T 캐스트 성공한 배열.
	//   GetLivingEnemiesOfType<T>(Out): IsAlive() 추가 필터.
	//   FindFirstEnemyOfType<T>()     : 첫 매칭 항목(없으면 nullptr).
	//   CountLivingEnemiesOfType<T>() : 생존 매칭 개수.
	//   ApplyDamageToAll<T>(...)      : 매칭된 적 전체에 ApplyDamage 한 방.


	template <typename T = AEnemyBase, typename Func>
	void ForEachEnemy(Func&& Predicate)
	{
		CompactStaleEntries();
		for (const TWeakObjectPtr<AEnemyBase>& Weak : RegisteredEnemies)
		{
			if (T* Typed = Cast<T>(Weak.Get()))
			{
				Predicate(Typed);
			}
		}
	}

	template <typename T = AEnemyBase, typename Func>
	void ForEachLivingEnemy(Func&& Predicate)
	{
		ForEachEnemy<T>([&Predicate](T* E) {
			if (IsValid(E) && E->IsAlive())
			{
				Predicate(E);
			}
		});
	}

	template <typename T = AEnemyBase>
	void GetEnemiesOfType(TArray<T*>& OutEnemies)
	{
		OutEnemies.Reset();
		ForEachEnemy<T>([&OutEnemies](T* E) { OutEnemies.Add(E); });
	}

	template <typename T = AEnemyBase>
	void GetLivingEnemiesOfType(TArray<T*>& OutEnemies)
	{
		OutEnemies.Reset();
		ForEachLivingEnemy<T>([&OutEnemies](T* E) { OutEnemies.Add(E); });
	}

	template <typename T = AEnemyBase>
	T* FindFirstEnemyOfType()
	{
		CompactStaleEntries();
		for (const TWeakObjectPtr<AEnemyBase>& Weak : RegisteredEnemies)
		{
			if (T* Typed = Cast<T>(Weak.Get()))
			{
				return Typed;
			}
		}
		return nullptr;
	}

	template <typename T = AEnemyBase>
	T* FindFirstLivingEnemyOfType()
	{
		CompactStaleEntries();
		for (const TWeakObjectPtr<AEnemyBase>& Weak : RegisteredEnemies)
		{
			T* Typed = Cast<T>(Weak.Get());
			if (Typed && Typed->IsAlive())
			{
				return Typed;
			}
		}
		return nullptr;
	}

	template <typename T = AEnemyBase>
	int32 CountLivingEnemiesOfType()
	{
		int32 Count = 0;
		ForEachLivingEnemy<T>([&Count](T*) { ++Count; });
		return Count;
	}

	/** 모든 매칭 적에 ApplyDamage 일괄 적용. PerformAttack 결과 위임 등 전투 일괄 처리용. */
	template <typename T = AEnemyBase>
	int32 ApplyDamageToAll(float Damage, AController* Instigator, AActor* Causer,
	                      TSubclassOf<UDamageType> DamageType = nullptr)
	{
		int32 Hit = 0;
		ForEachLivingEnemy<T>([&](T* E) {
			const float Applied = E->TakeDamage(Damage, FDamageEvent(DamageType), Instigator, Causer);
			if (Applied > 0.f)
			{
				++Hit;
			}
		});
		return Hit;
	}

	/** 반경 안 생존 적 수집(원형, 3D 거리). AOE/디버프 일괄 적용용. */
	template <typename T = AEnemyBase>
	void GetLivingEnemiesInRadius(const FVector& Center, float Radius, TArray<T*>& OutEnemies,
	                              bool bHorizontalOnly = false)
	{
		OutEnemies.Reset();
		const float R2 = Radius * Radius;
		ForEachLivingEnemy<T>([&](T* E) {
			const FVector P = E->GetActorLocation();
			const float D2 = bHorizontalOnly
				? FVector::DistSquared(FVector(P.X, P.Y, 0.f), FVector(Center.X, Center.Y, 0.f))
				: FVector::DistSquared(P, Center);
			if (D2 <= R2)
			{
				OutEnemies.Add(E);
			}
		});
	}

	template <typename T = AEnemyBase, typename Func>
	void ForEachLivingEnemyInRadius(const FVector& Center, float Radius, Func&& Predicate,
	                                bool bHorizontalOnly = false)
	{
		const float R2 = Radius * Radius;
		ForEachLivingEnemy<T>([&](T* E) {
			const FVector P = E->GetActorLocation();
			const float D2 = bHorizontalOnly
				? FVector::DistSquared(FVector(P.X, P.Y, 0.f), FVector(Center.X, Center.Y, 0.f))
				: FVector::DistSquared(P, Center);
			if (D2 <= R2)
			{
				Predicate(E);
			}
		});
	}

	/** 반경 안 모든 생존 적에 ApplyDamage. AOE 폭발 등 한 줄 처리. */
	template <typename T = AEnemyBase>
	int32 ApplyDamageInRadius(const FVector& Center, float Radius, float Damage,
	                          AController* Instigator, AActor* Causer,
	                          TSubclassOf<UDamageType> DamageType = nullptr,
	                          bool bHorizontalOnly = false)
	{
		int32 Hit = 0;
		ForEachLivingEnemyInRadius<T>(Center, Radius, [&](T* E) {
			const float Applied = E->TakeDamage(Damage, FDamageEvent(DamageType), Instigator, Causer);
			if (Applied > 0.f)
			{
				++Hit;
			}
		}, bHorizontalOnly);
		return Hit;
	}

	/** AEnemyBase가 직접 호출. 외부에서 임의 호출하지 않음. */
	void NotifyEnemyDamaged(AEnemyBase* Enemy, float DamageAmount, float CurrentHealth, float MaxHealth);
	void NotifyEnemyDied(AEnemyBase* Enemy);
	void NotifyEnemyAttackCommitted(AEnemyBase* Enemy, AActor* Target, float DamageAmount, UClass* DamageTypeClass = nullptr);
	void NotifyEnemyFSMStateChanged(AEnemyBase* Enemy, EEnemyAIState OldState, EEnemyAIState NewState);

private:
	TArray<TWeakObjectPtr<AEnemyBase>> RegisteredEnemies;

	void CompactStaleEntries();
};
