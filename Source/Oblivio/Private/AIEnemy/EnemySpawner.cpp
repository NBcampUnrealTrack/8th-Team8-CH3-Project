#include "AIEnemy/EnemySpawner.h"
#include "AIEnemy/CrawlingFleshMassEnemy.h"
#include "OblivioGameInstance.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "TimerManager.h"

// =============================================================================
// 웨이브 흐름: StartWave → PendingSpawns 채움 → SpawnNextEnemy 반복(타이머) →
//   스폰 시 OnEnemyDied 구독 → 전원 사망·큐 소진 시 CheckWaveCompleted
// =============================================================================

AEnemySpawner::AEnemySpawner()
{
	PrimaryActorTick.bCanEverTick = false;
}

// bAutoStart면 지정 웨이브부터 시작
void AEnemySpawner::BeginPlay()
{
	Super::BeginPlay();
	/*
	if (bAutoStart)
	{
		StartWave(AutoStartWaveIndex);
	}
	*/

	//추가: 인스턴스에서 층수를 받아와서 해당 층별로 맞는 몬스터를 스폰하고자 코드를 수정했습니다.
	//기존 코드는 주석처리만 했으니 확인해보시고 기존 게 더 설계 의도에 적합하시면 삭제하셔도 무방합니다.
	UOblivioGameInstance* GI = Cast<UOblivioGameInstance>(GetGameInstance());
	if (!GI) return;

	//층수에 따른 웨이브 인덱스 계산
	int32 TargetWaveIndex = 9 - GI->CurrentFloor;

	//모든 스포너에 해당 웨이브 시작 지시
	for (TActorIterator<AEnemySpawner> It(GetWorld()); It; ++It)
	{
		if (AEnemySpawner* Spawner = *It)
		{
			Spawner->StartWave(TargetWaveIndex);
		}
	}
}

// 진행 중이면 정리 후, 웨이브 데이터를 펼쳐 큐에 넣고 첫 스폰 또는 즉시 완료 판정
void AEnemySpawner::StartWave(int32 WaveIndex)
{
	if (!Waves.IsValidIndex(WaveIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s cannot start invalid wave index %d"), *GetNameSafe(this), WaveIndex);
		return;
	}

	StopWave();

	CurrentWaveIndex = WaveIndex;
	bWaveRunning = true;

	for (const FEnemyWaveEntry& Entry : Waves[WaveIndex].Entries)
	{
		if (!Entry.EnemyClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("%s wave %d has an empty enemy class"), *GetNameSafe(this), WaveIndex);
			continue;
		}

		for (int32 Index = 0; Index < Entry.Count; ++Index)
		{
			FPendingEnemySpawn PendingSpawn;
			PendingSpawn.EnemyClass = Entry.EnemyClass;
			PendingSpawn.SpawnInterval = Entry.SpawnInterval;
			PendingSpawns.Add(PendingSpawn);
		}
	}

	OnWaveStarted.Broadcast(CurrentWaveIndex, PendingSpawns.Num());

	if (PendingSpawns.IsEmpty())
	{
		CheckWaveCompleted();
		return;
	}

	SpawnNextEnemy();
}

// 다음 인덱스 또는 루프 시 0번 웨이브
void AEnemySpawner::StartNextWave()
{
	const int32 NextWaveIndex = CurrentWaveIndex == INDEX_NONE ? 0 : CurrentWaveIndex + 1;
	if (Waves.IsValidIndex(NextWaveIndex))
	{
		StartWave(NextWaveIndex);
		return;
	}

	if (bLoopWaves && !Waves.IsEmpty())
	{
		StartWave(0);
	}
}

// 타이머·대기 큐·구독 해제. 이미 스폰된 적 액터는 월드에 그대로 둠
void AEnemySpawner::StopWave()
{
	GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
	PendingSpawns.Reset();

	for (TObjectPtr<AEnemyBase> Enemy : ActiveEnemies)
	{
		if (IsValid(Enemy))
		{
			Enemy->OnEnemyDied.RemoveDynamic(this, &AEnemySpawner::HandleEnemyDied);
		}
	}

	ActiveEnemies.Reset();
	bWaveRunning = false;
}

// 사망 시 리스트에서 제거 후 웨이브 종료 조건 검사
void AEnemySpawner::HandleEnemyDied(AEnemyBase* Enemy)
{
	if (Enemy)
	{
		Enemy->OnEnemyDied.RemoveDynamic(this, &AEnemySpawner::HandleEnemyDied);
		ActiveEnemies.Remove(Enemy);
	}

	CheckWaveCompleted();
}

// 큐 맨 앞 한 마리 스폰·구독; 남은 큐 있으면 SpawnInterval 뒤 재호출
void AEnemySpawner::SpawnNextEnemy()
{
	if (!bWaveRunning || PendingSpawns.IsEmpty())
	{
		CheckWaveCompleted();
		return;
	}

	FPendingEnemySpawn PendingSpawn = PendingSpawns[0];
	PendingSpawns.RemoveAt(0);

	UWorld* World = GetWorld();
	if (!World || !PendingSpawn.EnemyClass)
	{
		CheckWaveCompleted();
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AEnemyBase* SpawnedEnemy = World->SpawnActor<AEnemyBase>(PendingSpawn.EnemyClass, GetSpawnTransform(), SpawnParameters);
	if (SpawnedEnemy)
	{
		SpawnedEnemy->OnEnemyDied.AddDynamic(this, &AEnemySpawner::HandleEnemyDied);
		ActiveEnemies.Add(SpawnedEnemy);
		if (ACrawlingFleshMassEnemy* Crawler = Cast<ACrawlingFleshMassEnemy>(SpawnedEnemy))
		{
			Crawler->InitializeSwarmMembership(this);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("%s failed to spawn enemy class %s"),
			*GetNameSafe(this), *GetNameSafe(PendingSpawn.EnemyClass.Get()));
	}

	if (!PendingSpawns.IsEmpty())
	{
		const float NextDelay = FMath::Max(PendingSpawn.SpawnInterval, KINDA_SMALL_NUMBER);
		GetWorldTimerManager().SetTimer(SpawnTimerHandle, this, &AEnemySpawner::SpawnNextEnemy, NextDelay, false);
	}
	else
	{
		OnWaveSpawnQueueEmptied.Broadcast(CurrentWaveIndex, this);
		CheckWaveCompleted();
	}
}

// 스폰 포인트 중 유효한 것 무작위, 없으면 스포너 Transform
FTransform AEnemySpawner::GetSpawnTransform() const
{
	if (!SpawnPoints.IsEmpty())
	{
		const int32 SpawnPointIndex = FMath::RandRange(0, SpawnPoints.Num() - 1);
		if (IsValid(SpawnPoints[SpawnPointIndex]))
		{
			return SpawnPoints[SpawnPointIndex]->GetActorTransform();
		}
	}

	return GetActorTransform();
}

// 웨이브 실행 중인데 큐·활성 적 모두 비었을 때만 완료 이벤트
void AEnemySpawner::CheckWaveCompleted()
{
	if (!bWaveRunning || !PendingSpawns.IsEmpty() || !ActiveEnemies.IsEmpty())
	{
		return;
	}

	const int32 CompletedWaveIndex = CurrentWaveIndex;
	bWaveRunning = false;
	OnWaveCompleted.Broadcast(CompletedWaveIndex);
}
