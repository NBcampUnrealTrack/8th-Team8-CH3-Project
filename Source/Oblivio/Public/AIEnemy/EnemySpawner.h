#pragma once

// =============================================================================
// AEnemySpawner — 웨이브별로 적 클래스·마릿수·스폰 간격을 큐에 넣고 순차 스폰.
// · SpawnPoints: 비어 있으면 스포너 액터 위치/회전 사용, 있으면 무작위 한 지점
// · 활성 적은 OnEnemyDied 바인딩으로 추적 → 대기 큐·살아 있는 적 모두 없을 때 웨이브 완료
// =============================================================================

class AEnemySpawner;

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AIEnemy/EnemyBase.h"
#include "EnemySpawner.generated.h"

/** 웨이브 한 줄: 동일 클래스를 Count번, 각 스폰 사이 SpawnInterval(초) */
USTRUCT(BlueprintType)
struct FEnemyWaveEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave", meta = (AllowAbstract = "false"))
	TSubclassOf<AEnemyBase> EnemyClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave", meta = (ClampMin = "1"))
	int32 Count = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave", meta = (ClampMin = "0.0"))
	float SpawnInterval = 1.0f;
};

/** 하나의 웨이브 = 여러 Entry 순서대로 펼쳐서 스폰 큐에 넣음 */
USTRUCT(BlueprintType)
struct FEnemyWaveDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave")
	TArray<FEnemyWaveEntry> Entries;
};

/** (웨이브 인덱스, 이번 웨이브에 예정된 총 스폰 수) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEnemyWaveStartedSignature, int32, WaveIndex, int32, TotalEnemies);
/** 방금 끝난 웨이브 인덱스 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEnemyWaveCompletedSignature, int32, WaveIndex);
/** 이번 웨이브의 스폰 대기 큐가 모두 소진된 직후(마지막 스폰 1건 처리 후). 활성 적이 남아 있어도 호출된다. swarm 동기용. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEnemyWaveSpawnQueueEmptiedSignature, int32, WaveIndex, AEnemySpawner*, Spawner);

UCLASS(Blueprintable)
class OBLIVIO_API AEnemySpawner : public AActor
{
	GENERATED_BODY()

public:
	AEnemySpawner();

	/** 지정 웨이브 인덱스로 큐 구성 후 스폰 시작(진행 중이면 Stop 후 재시작) */
	UFUNCTION(BlueprintCallable, Category = "Wave")
	void StartWave(int32 WaveIndex);

	/** 현재 다음 웨이브, 마지막이면 bLoopWaves면 0번으로 */
	UFUNCTION(BlueprintCallable, Category = "Wave")
	void StartNextWave();

	/** 타이머·대기 큐·활성 적 리스트 정리(삭제 아님, 구독만 해제) */
	UFUNCTION(BlueprintCallable, Category = "Wave")
	void StopWave();

	UFUNCTION(BlueprintCallable, Category = "Wave")
	bool IsWaveRunning() const { return bWaveRunning; }

	UPROPERTY(BlueprintAssignable, Category = "Wave|Events")
	FEnemyWaveStartedSignature OnWaveStarted;

	UPROPERTY(BlueprintAssignable, Category = "Wave|Events")
	FEnemyWaveCompletedSignature OnWaveCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Wave|Events")
	FEnemyWaveSpawnQueueEmptiedSignature OnWaveSpawnQueueEmptied;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave")
	TArray<FEnemyWaveDefinition> Waves;

	/** 비어 있으면 스포너 자기 Transform으로 스폰 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave")
	TArray<TObjectPtr<AActor>> SpawnPoints;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave")
	bool bAutoStart = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave", meta = (ClampMin = "0"))
	int32 AutoStartWaveIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave")
	bool bLoopWaves = false;

private:
	/** StartWave에서 펼쳐 넣는 스폰 한 건(다음 스폰까지 대기 시간 포함) */
	struct FPendingEnemySpawn
	{
		TSubclassOf<AEnemyBase> EnemyClass;
		float SpawnInterval = 1.0f;
	};

	UFUNCTION()
	void HandleEnemyDied(AEnemyBase* Enemy);

	void SpawnNextEnemy();
	FTransform GetSpawnTransform() const;
	/** 대기 큐 비었고 ActiveEnemies도 비었으면 OnWaveCompleted */
	void CheckWaveCompleted();

	UPROPERTY()
	TArray<TObjectPtr<AEnemyBase>> ActiveEnemies;

	TArray<FPendingEnemySpawn> PendingSpawns;
	FTimerHandle SpawnTimerHandle;
	int32 CurrentWaveIndex = INDEX_NONE;
	bool bWaveRunning = false;
};
