#include "AIEnemy/LuxeaterEnemy.h"

#include "OblivioCharacter.h"
#include "Weapon/Flashlight.h"
#include "OblivioComponents/LightAttackComponent.h"

#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "NiagaraFunctionLibrary.h"

#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

#include "Sound/SoundBase.h"

#include "Algo/Sort.h"

#include "AIEnemy/LuxeaterLaserMeshProbeActor.h"
namespace
{
	constexpr float LaserFaceInterpSpeedDegrees = 360.f;

	UNiagaraSystem* ResolveNiagaraWithComponentFallback(TObjectPtr<UNiagaraSystem> ClassAsset,
		UNiagaraComponent const* Comp)
	{
		if (IsValid(ClassAsset))
		{
			return ClassAsset.Get();
		}
		return Comp ? Comp->GetAsset() : nullptr;
	}

	static FVector GetPawnCylinderCenter(APawn const* Pawn)
	{
		if (!IsValid(Pawn))
		{
			return FVector::ZeroVector;
		}
		if (UCapsuleComponent const* Caps = Pawn->FindComponentByClass<UCapsuleComponent>())
		{
			return Caps->GetComponentLocation();
		}
		return Pawn->GetActorLocation();
	}

	static FVector FlatDirectionTo(APawn const* Self, FVector const Target)
	{
		if (!IsValid(Self))
		{
			return FVector::ForwardVector;
		}
		FVector D = Target - Self->GetActorLocation();
		D.Z = 0.f;
		if (!D.Normalize())
		{
			return FVector::ForwardVector;
		}
		return D;
	}

	/** 무기·부착 액터에 먼저 맞을 때 캡슐 피해 대상과 맞춘다(WeaponBase는 기본 TakeDamage≈0). */
	static AOblivioCharacter* ResolvePlayerFromLaserHitActor(AActor* HitActor)
	{
		AActor* Cursor = HitActor;
		for (int32 Depth = 0; Depth < 16 && IsValid(Cursor); ++Depth)
		{
			if (AOblivioCharacter* const P = Cast<AOblivioCharacter>(Cursor))
			{
				return P;
			}
			AActor* const Attached = Cursor->GetAttachParentActor();
			if (IsValid(Attached) && Attached != Cursor)
			{
				Cursor = Attached;
				continue;
			}
			AActor* const Own = Cursor->GetOwner();
			if (IsValid(Own) && Own != Cursor)
			{
				Cursor = Own;
				continue;
			}
			break;
		}
		return nullptr;
	}

	static AOblivioCharacter* ResolvePlayerFromLaserPrimChain(UPrimitiveComponent* Prim)
	{
		for (USceneComponent* Scene = Prim; IsValid(Scene); Scene = Scene->GetAttachParent())
		{
			if (AActor* const Owner = Scene->GetOwner(); IsValid(Owner))
			{
				if (AOblivioCharacter* const P = Cast<AOblivioCharacter>(Owner))
				{
					return P;
				}
			}
		}
		return nullptr;
	}

	static AOblivioCharacter* ResolvePlayerFromLaserHit(FHitResult const& Hit)
	{
		if (UPrimitiveComponent* const Prim = Hit.Component.Get())
		{
			if (AOblivioCharacter* const P = ResolvePlayerFromLaserPrimChain(Prim))
			{
				return P;
			}
			if (AActor* const Own = Prim->GetOwner(); IsValid(Own))
			{
				if (AOblivioCharacter* const P = ResolvePlayerFromLaserHitActor(Own))
				{
					return P;
				}
			}
		}
		if (AActor* const A = Hit.GetActor(); IsValid(A))
		{
			return ResolvePlayerFromLaserHitActor(A);
		}
		return nullptr;
	}

	/**
	 * 보스 레이저는 ApplyDamage→APawn::ShouldTakeDamage에서 꺼지는 경우가 있다
	 * (로컬 역할, AuthGameMode 없음, 멀티 추적 등). 피해는 UI/이벤트용 ApplyHealth와 동일 경로로 보낸다.
	 * 실제 차감은 OWNING(네트워크 권위) 액터에서만 수행한다.
	 */
	static void ApplyLuxeaterLaserDamageToPlayer(AOblivioCharacter* Char, float DamageAmount)
	{
		if (!IsValid(Char) || DamageAmount <= KINDA_SMALL_NUMBER || Char->bIsDead)
		{
			return;
		}
		// 순수 네트워크 클라이언트 창: 서버 권한 폰만 체력 반영(로컬 프록시에 Apply해도 의미 없음).
		UWorld* const W = Char->GetWorld();
		if (W && W->GetNetMode() == NM_Client && !Char->HasAuthority())
		{
			return;
		}
		Char->ApplyHealth(DamageAmount);
	}
}

ALuxeaterEnemy::ALuxeaterEnemy()
{
	LightStunBuildupSeconds = LuxuryLightExposureNeverStuns;

	MaxHealth = 600.0f;
	CurrentHealth = MaxHealth;
	MoveSpeed = 180.0f;
	ChaseMoveSpeed = 230.0f;
	AttackDamage = 0.0f;
	AttackRange = 1.0f;
	AttackCooldown = 1.2f;
	ChaseAcceptanceRadius = 65.0f;
	ChaseProximityBuffer = 45.0f;
	bAggroUseHorizontalDistance = true;
	bEnableIdleWander = false;
	bEnableLightTracking = false;

	LightAbsorbNiagara = CreateDefaultSubobject<UNiagaraComponent>(TEXT("LightAbsorbNiagara"));
	LightAbsorbNiagara->SetupAttachment(GetMesh());
	LightAbsorbNiagara->bAutoActivate = false;

	LaserChargeNiagara = CreateDefaultSubobject<UNiagaraComponent>(TEXT("LaserChargeNiagara"));
	LaserChargeNiagara->SetupAttachment(GetMesh());
	LaserChargeNiagara->bAutoActivate = false;
}

void ALuxeaterEnemy::StopLuxeaterChannelSfxForRestart(TObjectPtr<UAudioComponent>& Comp)
{
	if (UAudioComponent* const A = Comp.Get())
	{
		A->Stop();
	}
	Comp = nullptr;
}

void ALuxeaterEnemy::FadeOutLuxeaterChannelSfxOnCastEnd(TObjectPtr<UAudioComponent>& Comp)
{
	UAudioComponent* const A = Comp.Get();
	if (IsValid(A))
	{
		EAudioComponentPlayState const State = A->GetPlayState();
		if (State == EAudioComponentPlayState::Playing || State == EAudioComponentPlayState::FadingIn)
		{
			A->FadeOut(FMath::Max(0.05f, LuxeaterChannelSfxFadeOutSeconds), 0.f,
				EAudioFaderCurve::Linear);
		}
		else if (State != EAudioComponentPlayState::FadingOut)
		{
			A->Stop();
		}
	}
	Comp = nullptr;
}

void ALuxeaterEnemy::BeginPlay()
{
	BaseMoveSpeed = MoveSpeed;
	BaseChaseMoveSpeed = ChaseMoveSpeed > KINDA_SMALL_NUMBER ? ChaseMoveSpeed : MoveSpeed;
	InitialScale = GetActorScale3D();
	CurrentScaleMultiplier = 1.0f;
	TargetScaleFromAbsorb = 1.0f;

	LightAbsorbStacks = 0;

	ApplyBuffFromAbsorbStacks();

	FloatBaseMeshRelativeZ = GetMesh() ? GetMesh()->GetRelativeLocation().Z : 0.0f;
	FloatTime = FMath::FRandRange(0.0f, 2.0f * UE_PI);

	if (!bOverrideComputedMaxHealth && !bHealthMirroredFromExternalSystem)
	{
		TryComputeMaxHealthFromPlayerFlashlight();
	}

	if (UWorld* World = GetWorld())
	{
		const double Now = World->GetTimeSeconds();
		NextLaserAttackTimeSeconds = Now;
		NextLightAbsorbAvailableTimeSeconds = Now;
	}

	Super::BeginPlay();
}

void ALuxeaterEnemy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopLuxeaterChannelSfxForRestart(LightAbsorbCastSfxPlaying);
	StopLuxeaterChannelSfxForRestart(LaserChargeCastSfxPlaying);
	Super::EndPlay(EndPlayReason);
}

void ALuxeaterEnemy::Tick(float DeltaSeconds)
{
	TickBossAbilities(DeltaSeconds);
	Super::Tick(DeltaSeconds);

	if (!FMath::IsNearlyEqual(CurrentScaleMultiplier, TargetScaleFromAbsorb, 0.0001f))
	{
		CurrentScaleMultiplier =
			FMath::FInterpTo(CurrentScaleMultiplier, TargetScaleFromAbsorb, DeltaSeconds, ScaleInterpSpeed);
		SetActorScale3D(InitialScale * CurrentScaleMultiplier);
	}

	if (FloatAmplitude > KINDA_SMALL_NUMBER && IsAlive())
	{
		FloatTime += DeltaSeconds * FloatSpeed;
		if (USkeletalMeshComponent* MeshComp = GetMesh())
		{
			FVector RelativeLoc = MeshComp->GetRelativeLocation();
			RelativeLoc.Z = FloatBaseMeshRelativeZ + FMath::Sin(FloatTime) * FloatAmplitude;
			MeshComp->SetRelativeLocation(RelativeLoc);
		}
	}

	RefreshWalkSpeedFromSources();
}

void ALuxeaterEnemy::RefreshWalkSpeedFromSources()
{
	if (!IsAlive() || !GetCharacterMovement())
	{
		Super::RefreshWalkSpeedFromSources();
		return;
	}

	if (ShouldSuppressAILocomotion())
	{
		GetCharacterMovement()->MaxWalkSpeed = 0.0f;
		return;
	}

	ApplyBuffFromAbsorbStacks();
	Super::RefreshWalkSpeedFromSources();
}

bool ALuxeaterEnemy::ShouldSuppressAILocomotion() const
{
	return bLaserChargeActive || bLightAbsorbChanneling;
}

void ALuxeaterEnemy::ApplyCCSlow(float /*SpeedMultiplier*/, float /*Duration*/)
{
}

void ALuxeaterEnemy::ApplyCCStun(float /*Duration*/)
{
}

void ALuxeaterEnemy::OnLightHit(float Intensity, float Duration)
{
	if (!IsAlive())
	{
		return;
	}

	Super::OnLightHit(Intensity, Duration);

	if (Intensity <= KINDA_SMALL_NUMBER || Duration <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	TryScheduleLightAbsorbFromHit();
}

void ALuxeaterEnemy::NotifyBossHealthChanged(float NewCurrentHealth, float NewMaxHealth)
{
	bHealthMirroredFromExternalSystem = true;
	MaxHealth = FMath::Max(1.0f, NewMaxHealth);
	CurrentHealth = FMath::Clamp(NewCurrentHealth, 0.0f, MaxHealth);
	UpdateHealthPhase();
}

void ALuxeaterEnemy::NotifyEnemyDamageApplied(float /*AppliedDamage*/)
{
	UpdateHealthPhase();
}

bool ALuxeaterEnemy::HasValidAggroTarget() const
{
	if (bStickyAggroOnceTriggered && bAggroLatched)
	{
		return IsValid(TargetActor);
	}

	const bool bInRange = Super::HasValidAggroTarget();
	if (bInRange && bStickyAggroOnceTriggered)
	{
		bAggroLatched = true;
	}
	return bInRange;
}

void ALuxeaterEnemy::UpdateAttack()
{
}

void ALuxeaterEnemy::TryComputeMaxHealthFromPlayerFlashlight()
{
	if (bOverrideComputedMaxHealth || bHealthMirroredFromExternalSystem)
	{
		return;
	}

	float DmgTick = ReferenceFlashDamage;
	float Interval = FMath::Max(0.001f, ReferenceFlashAttackInterval);
	float DrainRate = FMath::Max(0.001f, ReferenceBatteryDepletionRate);
	float FocusPenalty = FMath::Lerp(1.0f, 1.5f, 0.5f);

	if (AActor* PlyActor = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		if (AOblivioCharacter* Oblivio = Cast<AOblivioCharacter>(PlyActor))
		{
			DrainRate = FMath::Max(0.001f, Oblivio->BatteryDepletionRate);
			FocusPenalty = FMath::Lerp(1.0f, 1.5f, Oblivio->CurrentFocusAlpha);

			if (AFlashlight* Flash = Cast<AFlashlight>(Oblivio->CurrentWeapon))
			{
				Interval = FMath::Max(0.001f, Flash->GetAttackInterval());
				if (ULightAttackComponent* Lac = Flash->FindComponentByClass<ULightAttackComponent>())
				{
					DmgTick = Lac->Damage;
				}
			}
			else if (AActor* WB = Oblivio->CurrentWeapon.Get())
			{
				if (ULightAttackComponent* Lac = WB->FindComponentByClass<ULightAttackComponent>())
				{
					DmgTick = Lac->Damage;
				}
			}
		}
	}

	const float SecondsForFullBatteryDrain = 100.0f / (DrainRate * FocusPenalty);
	const float EstimatedTicks = FMath::Max(1.0f, SecondsForFullBatteryDrain / Interval);
	const float EstimatedDamageBattery = EstimatedTicks * DmgTick;

	MaxHealth = FMath::Max(1.0f, MaxHealthBatteryDamageMultiplier * EstimatedDamageBattery);
}

void ALuxeaterEnemy::ApplyBuffFromAbsorbStacks()
{
	const float Stacks = static_cast<float>(FMath::Clamp(LightAbsorbStacks, 0, MaxLightAbsorbStacks));

	const float SpdMul =
		FMath::Max(1.0f, 1.0f + Stacks * BonusMoveSpeedMultiplierPerAbsorbStack);

	MoveSpeed = BaseMoveSpeed * SpdMul;
	ChaseMoveSpeed =
		BaseChaseMoveSpeed > KINDA_SMALL_NUMBER ? BaseChaseMoveSpeed * SpdMul : BaseMoveSpeed;

	TargetScaleFromAbsorb =
		FMath::Max(1.0f, 1.0f + Stacks * BonusScaleMultiplierPerAbsorbStack);
}

void ALuxeaterEnemy::UpdateHealthPhase()
{
	const float HealthPercent = MaxHealth > 0.0f ? CurrentHealth / MaxHealth : 0.0f;
	const int32 NewPhase = HealthPercent <= PhaseTwoHealthPercentThreshold ? 2 : 1;
	if (NewPhase == BossPhase)
	{
		return;
	}

	const int32 OldPhase = BossPhase;
	BossPhase = NewPhase;
	OnPhaseChanged.Broadcast(this, OldPhase, BossPhase);
	if (OldPhase == 1 && BossPhase == 2)
	{
		OnEnteredPhaseTwo.Broadcast(this);
	}
}

void ALuxeaterEnemy::TryScheduleLightAbsorbFromHit()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (LightAbsorbStacks >= MaxLightAbsorbStacks)
	{
		return;
	}

	const double Now = World->GetTimeSeconds();
	if (Now < NextLightAbsorbAvailableTimeSeconds)
	{
		return;
	}

	if (bLaserChargeActive || bLightAbsorbChanneling)
	{
		return;
	}

	bPendingLightAbsorbRequest = true;
}

void ALuxeaterEnemy::BeginLightAbsorbChannel()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	bPendingLightAbsorbRequest = false;
	bLightAbsorbChanneling = true;
	LightAbsorbChannelEndTimeSeconds = World->GetTimeSeconds() + LightAbsorbCastSeconds;

	StopLuxeaterChannelSfxForRestart(LightAbsorbCastSfxPlaying);
	if (IsValid(LightAbsorbSound))
	{
		LightAbsorbCastSfxPlaying =
			UGameplayStatics::SpawnSoundAtLocation(this, LightAbsorbSound, GetLaserTraceOrigin());
	}

	if (LightAbsorbNiagara)
	{
		if (UNiagaraSystem* Fx = ResolveNiagaraWithComponentFallback(LightAbsorbNiagaraSystem,
				LightAbsorbNiagara.Get()))
		{
			LightAbsorbNiagara->SetAsset(Fx);
			LightAbsorbNiagara->Activate(true);
		}
	}

	FaceAggroOrPlayerYaw(0.f, 0.f, true);
}

void ALuxeaterEnemy::FinishLightAbsorbChannel()
{
	UWorld* World = GetWorld();
	if (!World || !IsAlive())
	{
		return;
	}

	bLightAbsorbChanneling = false;

	if (LightAbsorbStacks < MaxLightAbsorbStacks)
	{
		LightAbsorbStacks++;
		OnLightAbsorbed.Broadcast(
			this, 1.f, static_cast<float>(LightAbsorbStacks));
	}

	ApplyBuffFromAbsorbStacks();
	RefreshWalkSpeedFromSources();

	NextLightAbsorbAvailableTimeSeconds = World->GetTimeSeconds() + LightAbsorbCooldownSeconds;

	FadeOutLuxeaterChannelSfxOnCastEnd(LightAbsorbCastSfxPlaying);

	if (LightAbsorbNiagara)
	{
		LightAbsorbNiagara->DeactivateImmediate();
	}
}

bool ALuxeaterEnemy::TryBeginLaserCharge()
{
	UWorld* World = GetWorld();
	if (!World || !HasValidAggroTarget())
	{
		return false;
	}

	if (bLaserChargeActive || bLightAbsorbChanneling)
	{
		return false;
	}

	const double Now = World->GetTimeSeconds();
	if (Now < NextLaserAttackTimeSeconds)
	{
		return false;
	}

	bLaserChargeActive = true;
	LaserChargeEndTimeSeconds = Now + LaserChargeSeconds;

	StopLuxeaterChannelSfxForRestart(LaserChargeCastSfxPlaying);
	if (IsValid(LaserChargeSound))
	{
		LaserChargeCastSfxPlaying =
			UGameplayStatics::SpawnSoundAtLocation(this, LaserChargeSound, GetLaserTraceOrigin());
	}

	if (LaserChargeNiagara)
	{
		if (UNiagaraSystem* Fx = ResolveNiagaraWithComponentFallback(LaserChargeNiagaraSystem,
				LaserChargeNiagara.Get()))
		{
			LaserChargeNiagara->SetAsset(Fx);
			LaserChargeNiagara->Activate(true);
		}
	}

	return true;
}

FVector ALuxeaterEnemy::GetLaserTraceOrigin() const
{
	if (USkeletalMeshComponent const* SkelMesh = GetMesh())
	{
		FName const Socket(LaserEyeSocketName);
		if (SkelMesh->DoesSocketExist(Socket))
		{
			return SkelMesh->GetSocketTransform(Socket).GetLocation();
		}
	}
	return GetActorLocation() + LaserOriginOffsetNoSocket;
}

void ALuxeaterEnemy::FaceAggroOrPlayerYaw(float const DeltaSeconds, float const InterpSpeedDegPerSec,
	bool const bImmediate)
{
	UWorld* const World = GetWorld();
	if (!World)
	{
		return;
	}

	FVector AimLoc = FVector::ZeroVector;
	bool bHasTarget = false;

	if (AActor* Aim = TargetActor.Get())
	{
		if (APawn* AimPawn = Cast<APawn>(Aim))
		{
			AimLoc = GetPawnCylinderCenter(AimPawn);
		}
		else
		{
			AimLoc = Aim->GetActorLocation();
		}
		bHasTarget = true;
	}
	else if (APawn* const P = UGameplayStatics::GetPlayerPawn(World, 0))
	{
		AimLoc = GetPawnCylinderCenter(P);
		bHasTarget = true;
	}

	if (!bHasTarget)
	{
		return;
	}

	const FVector DesiredDir = FlatDirectionTo(this, AimLoc);
	const FRotator TargetYaw(0.f, FRotationMatrix::MakeFromX(DesiredDir).Rotator().Yaw, 0.f);

	if (bImmediate || DeltaSeconds <= 0.f || InterpSpeedDegPerSec <= KINDA_SMALL_NUMBER)
	{
		SetActorRotation(TargetYaw);
		return;
	}

	const FRotator NewRot =
		FMath::RInterpTo(GetActorRotation(), TargetYaw, DeltaSeconds, InterpSpeedDegPerSec);
	SetActorRotation(NewRot);
}

void ALuxeaterEnemy::ExecuteLaserAttack()
{
	UWorld* World = GetWorld();
	if (!World || !IsAlive())
	{
		return;
	}

	if (!HasAuthority())
	{
		return;
	}

	AActor* AimActor = TargetActor.Get();
	if (!IsValid(AimActor))
	{
		AimActor = UGameplayStatics::GetPlayerPawn(World, 0);
	}
	if (!IsValid(AimActor))
	{
		return;
	}

	const FVector Origin = GetLaserTraceOrigin();
	FVector AimPoint = AimActor->GetActorLocation();

	if (APawn* AimPawn = Cast<APawn>(AimActor))
	{
		AimPoint = GetPawnCylinderCenter(AimPawn);
	}

	if (IsValid(LaserFireSound))
	{
		UGameplayStatics::PlaySoundAtLocation(this, LaserFireSound, Origin);
	}

	if (LaserProjectileProbeClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.Instigator = this;
		SpawnParams.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		if (ALuxeaterLaserMeshProbeActor* const Probe =
				World->SpawnActor<ALuxeaterLaserMeshProbeActor>(
					LaserProjectileProbeClass, FVector::ZeroVector, FRotator::ZeroRotator,
					SpawnParams))
		{
			Probe->ArmProbe(this, Origin, AimPoint, LaserProjectileSpeedUU,
				LaserProjectileImpactNiagaraSystem, LaserProjectileImpactNiagaraSystem,
				LaserProjectileProbeRadiusUU, LaserProjectileFallbackTimePad);
		}
		else
		{
			LaserRunTraceFallback(Origin);
		}
	}
	else
	{
		LaserRunTraceFallback(Origin);
	}
}

void ALuxeaterEnemy::LaserResolveProjectileImpact(FHitResult const& Hit)
{
	UPrimitiveComponent* const Prim = Hit.Component.Get();
	AActor* const HitActor = Hit.GetActor();

	if (HitActor == this)
	{
		return;
	}

	// GetActor가 비어도 Component→부착 사슬만으로 플레이어를 찾아야 하는 히트가 있다.
	if (!Prim && !IsValid(HitActor))
	{
		return;
	}

	if (AOblivioCharacter* Char = ResolvePlayerFromLaserHit(Hit))
	{
		const float EffectiveMax = FMath::Max(Char->MaxHealth, 1.f);
		const float dmg = LaserPlayerHealthFractionPerHit * EffectiveMax;
		ApplyLuxeaterLaserDamageToPlayer(Char, dmg);
		return;
	}

	if (!Prim || !IsValid(HitActor))
	{
		return;
	}

	const ECollisionChannel ObjType = Prim->GetCollisionObjectType();
	if (ObjType == ECC_WorldStatic)
	{
		return;
	}

	UGameplayStatics::ApplyDamage(HitActor, LaserCraftingBurstDamage, GetInstigatorController(), this,
		UDamageType::StaticClass());
}

void ALuxeaterEnemy::LaserRunTraceFallback(FVector const& TraceOriginWorld)
{
	UWorld* World = GetWorld();
	if (!World || !IsAlive())
	{
		return;
	}

	AActor* AimActor = TargetActor.Get();
	if (!IsValid(AimActor))
	{
		AimActor = UGameplayStatics::GetPlayerPawn(World, 0);
	}
	if (!IsValid(AimActor))
	{
		return;
	}

	FVector AimPoint = AimActor->GetActorLocation();
	if (APawn* AimPawn = Cast<APawn>(AimActor))
	{
		AimPoint = GetPawnCylinderCenter(AimPawn);
	}

	const FVector Origin = TraceOriginWorld;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(LuxeaterLaserTrace), false, this);
	Params.AddIgnoredActor(this);

	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjParams.AddObjectTypesToQuery(ECC_PhysicsBody);
	ObjParams.AddObjectTypesToQuery(ECC_Destructible);

	TArray<FHitResult> Hits;
	World->LineTraceMultiByObjectType(Hits, Origin, AimPoint, ObjParams, Params);

	FVector FxLoc = AimPoint;
	FVector FxNormal = (AimPoint - Origin).GetSafeNormal();
	if (FxNormal.IsNearlyZero())
	{
		FxNormal = FVector::UpVector;
	}

	if (Hits.Num() > 0)
	{
		Hits.Sort(
			[](FHitResult const& A, FHitResult const& B) { return A.Distance < B.Distance; });

		for (FHitResult const& PreviewHit : Hits)
		{
			AActor* const PrevA = PreviewHit.GetActor();
			if (!IsValid(PrevA) || PrevA == this)
			{
				continue;
			}
			FxLoc = PreviewHit.ImpactPoint;
			FVector const N = PreviewHit.ImpactNormal.GetSafeNormal();
			if (!N.IsNearlyZero())
			{
				FxNormal = N;
			}
			break;
		}

		TSet<const AActor*> ProcessedActors;
		for (FHitResult const& Hit : Hits)
		{
			UPrimitiveComponent* const Prim = Hit.Component.Get();
			AActor* const HitActor = Hit.GetActor();
			AActor* const DedupeKey = IsValid(HitActor)
										  ? HitActor
										  : (Prim ? Prim->GetOwner() : nullptr);
			if (!IsValid(DedupeKey) || DedupeKey == this || ProcessedActors.Contains(DedupeKey))
			{
				continue;
			}
			if (!Prim && !IsValid(HitActor))
			{
				continue;
			}
			ProcessedActors.Add(DedupeKey);

			if (AOblivioCharacter* Char = ResolvePlayerFromLaserHit(Hit))
			{
				const float EffectiveMax = FMath::Max(Char->MaxHealth, 1.f);
				const float dmg = LaserPlayerHealthFractionPerHit * EffectiveMax;
				ApplyLuxeaterLaserDamageToPlayer(Char, dmg);
				break;
			}

			if (!Prim)
			{
				continue;
			}

			const ECollisionChannel ObjType = Prim->GetCollisionObjectType();
			if (ObjType == ECC_WorldStatic)
			{
				break;
			}

			if (!IsValid(HitActor))
			{
				continue;
			}

			UGameplayStatics::ApplyDamage(HitActor, LaserCraftingBurstDamage,
				GetInstigatorController(), this, UDamageType::StaticClass());
			break;
		}
	}
	else if (AOblivioCharacter* Char = ResolvePlayerFromLaserHitActor(AimActor))
	{
		const float EffectiveMax = FMath::Max(Char->MaxHealth, 1.f);
		const float dmg = LaserPlayerHealthFractionPerHit * EffectiveMax;
		ApplyLuxeaterLaserDamageToPlayer(Char, dmg);
	}

	LaserSpawnImpactNiagaraAt(FxLoc, FxNormal);
}

void ALuxeaterEnemy::LaserSpawnImpactNiagaraAt(FVector const LocationWorld,
	FVector ImpactNormalWorld)
{
	UWorld* const World = GetWorld();
	if (!World || !IsValid(LaserProjectileImpactNiagaraSystem))
	{
		return;
	}

	FVector Normal = ImpactNormalWorld.GetSafeNormal();
	if (Normal.IsNearlyZero())
	{
		Normal = FVector::UpVector;
	}
	FRotator const Rot = UKismetMathLibrary::MakeRotFromZ(Normal);

	UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, LaserProjectileImpactNiagaraSystem,
		LocationWorld, Rot, FVector::OneVector, true, true);
}

void ALuxeaterEnemy::TickBossAbilities(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	if (!World || !IsAlive())
	{
		return;
	}

	const double Now = World->GetTimeSeconds();

	if (bLaserChargeActive)
	{
		if (Now >= LaserChargeEndTimeSeconds)
		{
			bLaserChargeActive = false;
			FadeOutLuxeaterChannelSfxOnCastEnd(LaserChargeCastSfxPlaying);
			if (LaserChargeNiagara)
			{
				LaserChargeNiagara->DeactivateImmediate();
			}
			ExecuteLaserAttack();
			NextLaserAttackTimeSeconds = Now + LaserCooldownSeconds;
		}
		else
		{
			FaceAggroOrPlayerYaw(DeltaSeconds, LaserFaceInterpSpeedDegrees, false);
		}
	}

	if (bLightAbsorbChanneling && !bLaserChargeActive)
	{
		FaceAggroOrPlayerYaw(DeltaSeconds, LightAbsorbFacePlayerYawInterpDegPerSec, false);
	}

	if (!bLaserChargeActive &&
		bLightAbsorbChanneling &&
		Now >= LightAbsorbChannelEndTimeSeconds)
	{
		FinishLightAbsorbChannel();
	}

	if (!bLaserChargeActive && TryBeginLaserCharge())
	{
		return;
	}

	if (!bLaserChargeActive && bPendingLightAbsorbRequest && Now >= NextLightAbsorbAvailableTimeSeconds &&
		LightAbsorbStacks < MaxLightAbsorbStacks && !bLightAbsorbChanneling)
	{
		BeginLightAbsorbChannel();
	}
}
