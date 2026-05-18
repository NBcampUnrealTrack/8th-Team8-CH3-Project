#include "AIEnemy/Tank/TankMembraneEmitterActor.h"
#include "AIEnemy/TankEnemy.h"
#include "AIEnemy/Tank/TankMembraneProjectile.h"
#include "Components/ArrowComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ATankMembraneEmitterActor::ATankMembraneEmitterActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

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

	MuzzleDirectionArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("MuzzleDirectionArrow"));
	MuzzleDirectionArrow->SetupAttachment(EmitterMesh);
	MuzzleDirectionArrow->bHiddenInGame = true;
	MuzzleDirectionArrow->SetUsingAbsoluteRotation(false);
	//MuzzleDirectionArrow->SetIsVisualizationComponent(true);
	MuzzleDirectionArrow->bIsEditorOnly = false;
	MuzzleDirectionArrow->SetArrowColor(FLinearColor(0.15f, 0.85f, 0.2f));
	MuzzleDirectionArrow->SetArrowLength(80.f);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderAsset(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* const CylinderMesh = CylinderAsset.Succeeded() ? CylinderAsset.Object.Get() : nullptr;
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicShapeMat(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	ProjectileTelegraphTubes.Reserve(3);
	for (int32 I = 0; I < 3; ++I)
	{
		FString const TubeName =
			I == 0 ? FString(TEXT("ProjectileTelegraphTube0"))
			: (I == 1 ? FString(TEXT("ProjectileTelegraphTube1"))
					  : FString(TEXT("ProjectileTelegraphTube2")));
		UStaticMeshComponent* const TubeComp =
			CreateDefaultSubobject<UStaticMeshComponent>(*TubeName);
		TubeComp->SetupAttachment(EmitterMesh);
		TubeComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		TubeComp->SetHiddenInGame(true);
		TubeComp->SetCastShadow(false);
		TubeComp->SetMobility(EComponentMobility::Movable);
		if (CylinderMesh)
		{
			TubeComp->SetStaticMesh(CylinderMesh);
		}
		if (BasicShapeMat.Succeeded())
		{
			TubeComp->SetMaterial(0, BasicShapeMat.Object);
		}
		TubeComp->SetTranslucentSortPriority(100);
		ProjectileTelegraphTubes.Add(TubeComp);
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

void ATankMembraneEmitterActor::HideProjectileTelegraphVisuals()
{
	const int32 N = ProjectileTelegraphTubes.Num();
	for (int32 Idx = 0; Idx < N; ++Idx)
	{
		UStaticMeshComponent* const Beam = ProjectileTelegraphTubes[Idx].Get();
		if (IsValid(Beam))
		{
			Beam->SetHiddenInGame(true);
		}
	}
}

void ATankMembraneEmitterActor::Multicast_ShowProjectileTelegraph_Implementation(
	FVector const TelegraphOriginWorld,
	FVector TelegraphBaseForwardNorm,
	float const TelegraphFanHalfDeg,
	float const TelegraphSpawnFwdCm,
	float const TelegraphSpawnUpCm,
	float const TelegraphRangeUU,
	float const TelegraphDurationSec)
{
	UWorld* const World = GetWorld();
	if (!World || ProjectileTelegraphTubes.Num() < 3)
	{
		return;
	}

	FVector BaseF = TelegraphBaseForwardNorm.GetSafeNormal();
	if (BaseF.IsNearlyZero())
	{
		return;
	}

	if (IsValid(ProjectileTelegraphMaterial))
	{
		for (int32 Idx = 0; Idx < ProjectileTelegraphTubes.Num(); ++Idx)
		{
			UStaticMeshComponent* const Beam = ProjectileTelegraphTubes[Idx].Get();
			if (IsValid(Beam))
			{
				Beam->SetMaterial(0, ProjectileTelegraphMaterial);
			}
		}
	}

	FVector const Up = FVector::UpVector;
	float const Yaws[3] = { -TelegraphFanHalfDeg, 0.f, TelegraphFanHalfDeg };
	float const FwdCm = FMath::Max(0.f, TelegraphSpawnFwdCm);
	float const UpCm = FMath::Max(0.f, TelegraphSpawnUpCm);
	float const LenUU = FMath::Max(10.f, TelegraphRangeUU);
	static constexpr float DefaultCylinderHeightUU = 100.f;
	float const RadScale = FMath::Max(0.05f, ProjectileTelegraphTubeRadiusCm / 50.f);

	for (int32 Idx = 0; Idx < 3; ++Idx)
	{
		UStaticMeshComponent* const Beam = ProjectileTelegraphTubes[Idx].Get();
		if (!IsValid(Beam))
		{
			continue;
		}

		FVector Dir = BaseF.RotateAngleAxis(Yaws[Idx], Up).GetSafeNormal();
		if (Dir.IsNearlyZero())
		{
			continue;
		}

		FVector const SpawnStart = TelegraphOriginWorld + Dir * FwdCm + Up * UpCm;
		FVector const MidWorld = SpawnStart + Dir * (LenUU * 0.5f);

		Beam->SetWorldLocation(MidWorld);

		FQuat const AlignCylinderZToTrajectory = FQuat::FindBetweenNormals(FVector::UpVector, Dir);
		Beam->SetWorldRotation(AlignCylinderZToTrajectory);
		Beam->SetWorldScale3D(FVector(RadScale, RadScale, FMath::Max(1.f, LenUU / DefaultCylinderHeightUU)));
		Beam->SetHiddenInGame(false);
	}

	World->GetTimerManager().ClearTimer(ProjectileTelegraphHideTimerHandle);
	World->GetTimerManager().SetTimer(ProjectileTelegraphHideTimerHandle, this,
		&ATankMembraneEmitterActor::HideProjectileTelegraphVisuals, FMath::Max(TelegraphDurationSec, 0.05f),
		false);
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
		World->GetTimerManager().ClearTimer(ProjectileTelegraphHideTimerHandle);
		World->GetTimerManager().ClearTimer(DeferredDestroyAfterTelegraphTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void ATankMembraneEmitterActor::AppendLaunchRotOffsetToNormalizedForward(FVector& InOutForward,
	FRotator const OffsetDegrees)
{
	if (OffsetDegrees.IsNearlyZero(KINDA_SMALL_NUMBER))
	{
		return;
	}
	FRotator R = InOutForward.Rotation();
	R += OffsetDegrees;
	InOutForward = R.Vector().GetSafeNormal();
}

void ATankMembraneEmitterActor::FaceAggroTowardTargetActor()
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

	FVector const From = GetActorLocation();
	FVector Delta = AimWorld - From;

	if (bFlattenAimToHorizontalPlaneWhenFacingAggro)
	{
		Delta.Z = 0.f;
		if (!Delta.Normalize())
		{
			return;
		}
		SetActorRotation(Delta.Rotation());
		return;
	}

	const FRotator LookAt = UKismetMathLibrary::FindLookAtRotation(From, AimWorld);
	SetActorRotation(FRotator(LookAt.Pitch, LookAt.Yaw, 0.f));
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

	// 스폰 포인트 트랜스폼에는 보통 Scale=(1,1,1) 이 들어간다. SetActorTransform 전체를 쓰면
	// BP 클래스 기본값·디테일에서 키운 액터/메시 스케일이 매번 덮어씌워지므로 스케일만 유지한다.
	{
		const FVector PreservedScale3D = GetActorTransform().GetScale3D();
		FTransform Placed = SpawnWorld;
		Placed.SetScale3D(PreservedScale3D);
		SetActorTransform(Placed);
	}

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

	// 매 볼리마다(옵션) 플레이어 방향으로 재조준 — 이동하는 플레이어 추적
	if (bFaceAggroTargetEachVolley)
	{
		FaceAggroTowardTargetActor();
	}
	SpawnFanProjectiles();

	++VolleysFiredSoFar;

	if (VolleysFiredSoFar >= VolleyCount)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Membrane] %s 모든 볼리 완료 (%d 회) — 탱크 알림·정리"),
			*GetNameSafe(this), VolleysFiredSoFar);

		StopVolleyTimerAndNotifyTankFinished();

		// 마지막 볼리 직후 Destroy 하면 클라 텔레그래프 멀티캐스트가 도착 전 액터가 사라져
		// 라인 표시가 누락된다. 노출 시간만큼 Destroy 만 지연.
		float TelegraphPadSeconds = 0.f;
		if (bShowProjectilePathTelegraph && ProjectileTelegraphVisibleSeconds > KINDA_SMALL_NUMBER
			&& ProjectileTelegraphRangeUU > SMALL_NUMBER)
		{
			TelegraphPadSeconds =
				FMath::Clamp(ProjectileTelegraphVisibleSeconds + 0.06f, 0.06f, 10.f);
		}

		if (TelegraphPadSeconds > KINDA_SMALL_NUMBER && GetWorld())
		{
			GetWorld()->GetTimerManager().ClearTimer(DeferredDestroyAfterTelegraphTimerHandle);
			GetWorld()->GetTimerManager().SetTimer(DeferredDestroyAfterTelegraphTimerHandle, this,
				&ATankMembraneEmitterActor::OnDeferredDestroyAfterTelegraph_TimerFired, TelegraphPadSeconds,
				false);
			return;
		}

		DestroyMembraneEmitterNow();
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

	UArrowComponent const* const Muzzle = MuzzleDirectionArrow;
	FVector BaseForward = FVector::ForwardVector;
	if (IsValid(Muzzle))
	{
		BaseForward = Muzzle->GetForwardVector();
		if (bInvertMuzzleDirectionArrowForward)
		{
			BaseForward *= -1.f;
		}
	}
	else
	{
		BaseForward = GetActorForwardVector();
	}
	if (!BaseForward.Normalize())
	{
		BaseForward = FVector::ForwardVector;
	}

	AppendLaunchRotOffsetToNormalizedForward(BaseForward, ProjectileLaunchRotatorOffsetDegrees);

	FVector const Up = FVector::UpVector;
	float const Yaws[3] = { -FanHalfAngleDeg, 0.f, FanHalfAngleDeg };

	FActorSpawnParameters Params;
	Params.Owner = TankOwner.Get();
	if (ATankEnemy* const T = TankOwner.Get())
	{
		Params.Instigator = T;
	}
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	FVector const Origin = IsValid(Muzzle) ? Muzzle->GetComponentLocation() : GetActorLocation();

	// Muzzle 오프셋: 양막 본체가 바닥/벽에 박혀 있어 투사체가 첫 프레임에 충돌→정지하는 문제 방지.
	// 또한 좁은 부채꼴(예: ±12°)에서 같은 Origin 에 3발이 스폰되면 서로 가까이 겹치므로 발사 축 따라 띄운다.
	float const ForwardOffset = FMath::Max(0.f, ProjectileSpawnForwardCm);
	float const UpOffset = FMath::Max(0.f, ProjectileSpawnUpCm);

	if (HasAuthority() && bShowProjectilePathTelegraph && ProjectileTelegraphVisibleSeconds > KINDA_SMALL_NUMBER
		&& ProjectileTelegraphRangeUU > SMALL_NUMBER && ProjectileTelegraphTubes.Num() >= 3)
	{
		Multicast_ShowProjectileTelegraph(
			Origin,
			BaseForward,
			FanHalfAngleDeg,
			ProjectileSpawnForwardCm,
			ProjectileSpawnUpCm,
			ProjectileTelegraphRangeUU,
			ProjectileTelegraphVisibleSeconds);
	}

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
		TEXT("[Membrane] %s SpawnFanProjectiles 결과 — 성공 %d / 3 (Origin=%s, BaseForward=%s)"),
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

void ATankMembraneEmitterActor::StopVolleyTimerAndNotifyTankFinished()
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(VolleyTimerHandle);
	}
	if (ATankEnemy* const T = TankOwner.Get())
	{
		T->NotifyTankMembraneEmitterFinished(this);
	}
}

void ATankMembraneEmitterActor::DestroyMembraneEmitterNow()
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DeferredDestroyAfterTelegraphTimerHandle);
	}
	Destroy();
}

void ATankMembraneEmitterActor::OnDeferredDestroyAfterTelegraph_TimerFired()
{
	DestroyMembraneEmitterNow();
}
