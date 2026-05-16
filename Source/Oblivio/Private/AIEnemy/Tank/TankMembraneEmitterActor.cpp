#include "AIEnemy/Tank/TankMembraneEmitterActor.h"
#include "AIEnemy/TankEnemy.h"
#include "AIEnemy/Tank/TankMembraneProjectile.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ATankMembraneEmitterActor::ATankMembraneEmitterActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SetReplicates(true);

	EmitterMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EmitterMesh"));
	SetRootComponent(EmitterMesh);
	EmitterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EmitterMesh->SetCastShadow(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereAsset(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereAsset.Succeeded())
	{
		EmitterMesh->SetStaticMesh(SphereAsset.Object);
		EmitterMesh->SetRelativeScale3D(FVector(0.35f));
	}
}

void ATankMembraneEmitterActor::OnMembraneEmitterAnimPhaseChanged_Implementation(
	ETankMembraneEmitterAnimPhase const NewPhase)
{
	(void)NewPhase;
}

void ATankMembraneEmitterActor::SetMembraneAnimPhase(ETankMembraneEmitterAnimPhase const NewPhase)
{
	if (!HasAuthority())
	{
		return;
	}
	Multicast_SetMembraneAnimPhase(NewPhase);
}

void ATankMembraneEmitterActor::Multicast_SetMembraneAnimPhase_Implementation(
	ETankMembraneEmitterAnimPhase const NewPhase)
{
	MembraneAnimPhase = NewPhase;
	OnMembraneEmitterAnimPhaseChanged(NewPhase);
}

void ATankMembraneEmitterActor::Multicast_PlayProjectileFireSfx_Implementation(FVector const& Location)
{
	if (!ProjectileFireSfx)
	{
		return;
	}
	UGameplayStatics::PlaySoundAtLocation(this, ProjectileFireSfx, Location,
		ProjectileFireSfxVolume, ProjectileFireSfxPitch);
}

void ATankMembraneEmitterActor::Multicast_PlayMembraneFirePulse_Implementation(FVector const& MuzzleLocationWorld)
{
	OnMembraneFirePulse(MuzzleLocationWorld);
}

void ATankMembraneEmitterActor::OnMembraneFirePulse_Implementation(FVector const& MuzzleLocationWorld)
{
	(void)MuzzleLocationWorld;

	if (PulseDecaySeconds <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	UMaterialInstanceDynamic* const MID = GetOrCreateEmitterPulseMID();
	if (!MID)
	{
		return;
	}

	MID->SetVectorParameterValue(PulseColorParameterName, PulseColor);

	if (UWorld* const World = GetWorld())
	{
		PulseStartedWorldSeconds = World->GetTimeSeconds();
		World->GetTimerManager().ClearTimer(PulseDecayTimerHandle);
		// 0.033s 간격 ≈ 30Hz 페이드. 짧은 펄스이므로 부담 없음.
		World->GetTimerManager().SetTimer(PulseDecayTimerHandle, this,
			&ATankMembraneEmitterActor::TickPulseFade, 0.033f, true);
	}
}

UMaterialInstanceDynamic* ATankMembraneEmitterActor::GetOrCreateEmitterPulseMID()
{
	if (!IsValid(EmitterMesh))
	{
		return nullptr;
	}
	if (IsValid(EmitterPulseMID))
	{
		return EmitterPulseMID;
	}
	UMaterialInterface* const Existing = EmitterMesh->GetMaterial(PulseMaterialElementIndex);
	if (!Existing)
	{
		return nullptr;
	}
	EmitterPulseMID = UMaterialInstanceDynamic::Create(Existing, this);
	if (EmitterPulseMID)
	{
		EmitterMesh->SetMaterial(PulseMaterialElementIndex, EmitterPulseMID);
	}
	return EmitterPulseMID;
}

void ATankMembraneEmitterActor::TickPulseFade()
{
	UWorld* const World = GetWorld();
	if (!World || !IsValid(EmitterPulseMID) || PulseDecaySeconds <= KINDA_SMALL_NUMBER)
	{
		if (World)
		{
			World->GetTimerManager().ClearTimer(PulseDecayTimerHandle);
		}
		return;
	}
	const float Elapsed = static_cast<float>(World->GetTimeSeconds() - PulseStartedWorldSeconds);
	const float Alpha = FMath::Clamp(Elapsed / PulseDecaySeconds, 0.f, 1.f);
	const FLinearColor Current = FMath::Lerp(PulseColor, PulseRestColor, Alpha);
	EmitterPulseMID->SetVectorParameterValue(PulseColorParameterName, Current);
	if (Alpha >= 1.f)
	{
		World->GetTimerManager().ClearTimer(PulseDecayTimerHandle);
	}
}

void ATankMembraneEmitterActor::BeginPlay()
{
	Super::BeginPlay();
	SetActorEnableCollision(false);
}

void ATankMembraneEmitterActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(VolleyTimerHandle);
		World->GetTimerManager().ClearTimer(PulseDecayTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void ATankMembraneEmitterActor::FaceAggroTargetHorizontal()
{
	FVector AimWorld = FVector::ZeroVector;
	bool bHaveAim = false;

	if (ATankEnemy* const T = TankOwner.Get())
	{
		if (AActor* const Aggro = T->GetTargetActor())
		{
			AimWorld = Aggro->GetActorLocation();
			bHaveAim = true;
		}
	}

	if (!bHaveAim)
	{
		if (APawn* const PP = UGameplayStatics::GetPlayerPawn(GetWorld(), 0))
		{
			AimWorld = PP->GetActorLocation();
			bHaveAim = true;
		}
	}

	if (!bHaveAim)
	{
		return;
	}

	FVector Delta = AimWorld - GetActorLocation();
	Delta.Z = 0.f;
	if (!Delta.Normalize())
	{
		return;
	}

	SetActorRotation(Delta.Rotation());
}

void ATankMembraneEmitterActor::ConfigureAndStartBurst(ATankEnemy* InTank, FTransform const& SpawnWorld,
	float const InFanHalfAngleDeg, float const InProjectileSpeedUU, float const InProjectileDamage,
	int32 const InVolleyCount, float const InVolleyIntervalSeconds,
	TSubclassOf<ATankMembraneProjectile> InProjectileClass)
{
	TankOwner = InTank;

	UE_LOG(LogTemp, Warning,
		TEXT("[Membrane] %s ConfigureAndStartBurst — Loc=%s | Tank Override(in): Fan=%.2f Speed=%.2f Volley=%d Interval=%.2f"
			 " | BP 기본값: Fan=%.2f Speed=%.2f Volley=%d Interval=%.2f Delay=%.2f"),
		*GetNameSafe(this), *SpawnWorld.GetLocation().ToString(),
		InFanHalfAngleDeg, InProjectileSpeedUU, InVolleyCount, InVolleyIntervalSeconds,
		FanHalfAngleDeg, ProjectileSpeedUU, VolleyCount, VolleyIntervalSeconds, FirstVolleyDelaySeconds);

	// 0/음수면 BP 기본값 유지. 양수만 Override 로 받는다.
	if (InFanHalfAngleDeg > 0.f)
	{
		FanHalfAngleDeg = InFanHalfAngleDeg;
	}
	if (InProjectileSpeedUU > 0.f)
	{
		ProjectileSpeedUU = InProjectileSpeedUU;
	}
	if (InVolleyCount > 0)
	{
		VolleyCount = InVolleyCount;
	}
	if (InVolleyIntervalSeconds > 0.f)
	{
		VolleyIntervalSeconds = InVolleyIntervalSeconds;
	}
	ProjectileDamage = InProjectileDamage;
	ProjectileClass = InProjectileClass;

	SetActorTransform(SpawnWorld);

	if (!HasAuthority() || !GetWorld())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Membrane] %s ConfigureAndStartBurst 종료 — 권한/월드 없음 (HasAuth=%d HasWorld=%d)"),
			*GetNameSafe(this), HasAuthority() ? 1 : 0, GetWorld() ? 1 : 0);
		return;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[Membrane] %s 발사 시작 — 최종 적용값: Fan=%.2f Speed=%.2f VolleyCount=%d Interval=%.2f Delay=%.2f"),
		*GetNameSafe(this), FanHalfAngleDeg, ProjectileSpeedUU, VolleyCount, VolleyIntervalSeconds, FirstVolleyDelaySeconds);

	/** 양막 등장 연출은 BP 단일 enum 분기(Spawn) 만 사용. */
	SetMembraneAnimPhase(ETankMembraneEmitterAnimPhase::Spawn);

	VolleysFiredSoFar = 0;

	if (FirstVolleyDelaySeconds <= KINDA_SMALL_NUMBER)
	{
		FireOneVolley();
	}
	else
	{
		ScheduleNextVolley(FirstVolleyDelaySeconds);
	}
}

void ATankMembraneEmitterActor::ScheduleNextVolley(float const DelaySeconds)
{
	UWorld* const World = GetWorld();
	if (!World || !HasAuthority())
	{
		return;
	}
	World->GetTimerManager().ClearTimer(VolleyTimerHandle);
	World->GetTimerManager().SetTimer(VolleyTimerHandle, this,
		&ATankMembraneEmitterActor::FireOneVolley, FMath::Max(DelaySeconds, 0.01f), false);
}

void ATankMembraneEmitterActor::FireOneVolley()
{
	if (!HasAuthority())
	{
		return;
	}

	// 양막 자체가 ApplyHealth 등으로 죽었을 수 있음 — 안전하게 종료
	if (!IsValid(this))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Membrane] %s FireOneVolley 호출됐지만 IsValid==false — 자가 파괴 후 타이머 잔존 의심"),
			*GetNameSafe(this));
		return;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[Membrane] %s 볼리 시작 — %d / %d  (Loc=%s)"),
		*GetNameSafe(this), VolleysFiredSoFar + 1, VolleyCount, *GetActorLocation().ToString());

	// 매 볼리마다 플레이어 방향으로 재조준 — 이동하는 플레이어 추적
	FaceAggroTargetHorizontal();
	SpawnFanProjectiles();

	++VolleysFiredSoFar;

	if (VolleysFiredSoFar >= VolleyCount)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Membrane] %s 모든 볼리 완료 (%d 회) — 자가 파괴"),
			*GetNameSafe(this), VolleysFiredSoFar);
		NotifyTankAndDestroy();
		return;
	}

	ScheduleNextVolley(VolleyIntervalSeconds);
}

void ATankMembraneEmitterActor::SpawnFanProjectiles()
{
	UWorld* const World = GetWorld();
	if (!World || !ProjectileClass)
	{
		return;
	}

	FVector BaseForward = GetActorForwardVector();
	BaseForward.Z = 0.f;
	if (!BaseForward.Normalize())
	{
		BaseForward = FVector::ForwardVector;
	}

	FVector const Up = FVector::UpVector;
	float const Yaws[3] = { -FanHalfAngleDeg, 0.f, FanHalfAngleDeg };

	FActorSpawnParameters Params;
	Params.Owner = TankOwner.Get();
	if (ATankEnemy* const T = TankOwner.Get())
	{
		Params.Instigator = T;
	}
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	FVector const Origin = GetActorLocation();

	// Muzzle 오프셋: 양막 본체가 바닥/벽에 박혀 있어 투사체가 첫 프레임에 충돌→정지하는 문제 방지.
	// 또한 좁은 부채꼴(예: ±12°)에서 같은 Origin 에 3발이 스폰되면 서로 가까이 겹치므로 전방으로 살짝 띄운다.
	float const ForwardOffset = FMath::Max(0.f, ProjectileSpawnForwardCm);
	float const UpOffset = FMath::Max(0.f, ProjectileSpawnUpCm);

	ATankMembraneProjectile* JustSpawned[3] = { nullptr, nullptr, nullptr };
	int32 SpawnedOk = 0;

	for (int32 I = 0; I < 3; ++I)
	{
		FVector Dir = BaseForward.RotateAngleAxis(Yaws[I], Up);
		if (!Dir.Normalize())
		{
			continue;
		}

		FVector const SpawnLoc = Origin + Dir * ForwardOffset + Up * UpOffset;
		FRotator const Facing = Dir.Rotation();
		ATankMembraneProjectile* const P =
			World->SpawnActor<ATankMembraneProjectile>(ProjectileClass, SpawnLoc, Facing, Params);
		if (IsValid(P))
		{
			P->InitializeFlight(ProjectileDamage, ProjectileSpeedUU, Dir);
			JustSpawned[I] = P;
			++SpawnedOk;

			// 발사 SFX — 부채꼴 3발 각각 1회. 모든 머신에서 위치 기반 재생.
			Multicast_PlayProjectileFireSfx(SpawnLoc);
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[Membrane] %s 투사체 SpawnActor 실패 — Dir=%s SpawnLoc=%s ProjectileClass=%s"),
				*GetNameSafe(this), *Dir.ToString(), *SpawnLoc.ToString(), *GetNameSafe(ProjectileClass));
		}
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[Membrane] %s SpawnFanProjectiles 결과 — 성공 %d / 3 (Origin=%s, Forward=%s)"),
		*GetNameSafe(this), SpawnedOk, *Origin.ToString(), *BaseForward.ToString());

	// 볼리당 1회 양막 본체 점등 펄스 — 발사 muzzle 위치(중앙 발 기준 대략값) 전달.
	if (SpawnedOk > 0)
	{
		FVector const MuzzleApprox = Origin + BaseForward * ForwardOffset + Up * UpOffset;
		Multicast_PlayMembraneFirePulse(MuzzleApprox);
	}

	// 같은 볼리 안에서 상호 무시(부채꼴 3발끼리 자폭 방지).
	for (int32 I = 0; I < 3; ++I)
	{
		ATankMembraneProjectile* const A = JustSpawned[I];
		if (!IsValid(A))
		{
			continue;
		}
		for (int32 J = I + 1; J < 3; ++J)
		{
			A->IgnoreOtherMembraneProjectile(JustSpawned[J]);
		}
	}

	// 이전 볼리 / 다른 양막의 살아있는 투사체와도 상호 무시(2 emitter × N 볼리 모두 안전).
	for (int32 I = 0; I < 3; ++I)
	{
		ATankMembraneProjectile* const NewProj = JustSpawned[I];
		if (!IsValid(NewProj))
		{
			continue;
		}
		for (int32 K = AliveSpawnedProjectiles.Num() - 1; K >= 0; --K)
		{
			ATankMembraneProjectile* const Old = AliveSpawnedProjectiles[K].Get();
			if (!IsValid(Old))
			{
				AliveSpawnedProjectiles.RemoveAtSwap(K);
				continue;
			}
			NewProj->IgnoreOtherMembraneProjectile(Old);
		}
		AliveSpawnedProjectiles.Add(NewProj);
	}
}

void ATankMembraneEmitterActor::NotifyTankAndDestroy()
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(VolleyTimerHandle);
	}
	if (ATankEnemy* const T = TankOwner.Get())
	{
		T->NotifyTankMembraneEmitterFinished(this);
	}
	Destroy();
}
