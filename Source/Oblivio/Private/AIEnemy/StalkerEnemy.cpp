#include "AIEnemy/StalkerEnemy.h"
#include "OblivioCharacter.h"
#include "Components/AudioComponent.h"
#include "Components/SpotLightComponent.h"

AStalkerEnemy::AStalkerEnemy()
{
	StalkerAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("StalkerStealthAudio"));
	StalkerAudioComponent->SetupAttachment(RootComponent);
	StalkerAudioComponent->bAutoActivate = false;
	StalkerAudioComponent->bStopWhenOwnerDestroyed = true;

	// 절름발이 연출: 비추격 느림 / 추격·기습은 빠름 (기획서 스토커 프로필)
	MaxHealth = 95.0f;
	CurrentHealth = MaxHealth;
	MoveSpeed = 175.0f;
	ChaseMoveSpeed = 480.0f;
	AttackDamage = 16.0f;
	AttackRange = 150.0f;
	AttackCooldown = 1.55f;
	ChaseAcceptanceRadius = 55.0f;
	ChaseProximityBuffer = 48.0f;
	AggroRadius = 1100.0f;        // 0이면 항상 추격 → 배회 불가. 반드시 양수 유지.
	CorpseLifeSpan = 3.0f;
	InvestigateStimulusTimeout = 18.0f;

	// 어그로 밖에서 반드시 배회하도록 명시
	bEnableIdleWander = true;
}

void AStalkerEnemy::BeginPlay()
{
	Super::BeginPlay();

	StealthAudioEnvelope = 0.0f;

	if (StalkerAudioComponent && StalkerRevealLoopSound)
	{
		StalkerAudioComponent->SetSound(StalkerRevealLoopSound);
	}

	UpdateFlashlightReveal();
	UpdateStalkerStealthAudioEnvelope(0.0f);
}

void AStalkerEnemy::Tick(float DeltaSeconds)
{
	if (ChaseBurstTimeRemaining > 0.0f)
	{
		ChaseBurstTimeRemaining = FMath::Max(0.0f, ChaseBurstTimeRemaining - DeltaSeconds);
	}
	Super::Tick(DeltaSeconds);
	UpdateFlashlightReveal();
	UpdateStalkerStealthAudioEnvelope(DeltaSeconds);
}

void AStalkerEnemy::Die()
{
	ApplyFlashlightReveal(true);
	Super::Die();

	StealthAudioEnvelope = 0.0f;
	if (StalkerAudioComponent)
	{
		StalkerAudioComponent->Stop();
	}
}

void AStalkerEnemy::NotifyEnemyStateChanged(EEnemyAIState OldState, EEnemyAIState NewState)
{
	if (NewState != EEnemyAIState::Chase && NewState != EEnemyAIState::Attack)
	{
		ChaseBurstTimeRemaining = 0.0f;
	}

	if (NewState == EEnemyAIState::Chase)
	{
		const bool bFromCombatLocomotion = (OldState == EEnemyAIState::Chase || OldState == EEnemyAIState::Attack);
		if (!bFromCombatLocomotion && ChaseBurstDuration > KINDA_SMALL_NUMBER)
		{
			if (!bChaseBurstOnlyFromBehind || ShouldTriggerChaseBurstFromBehind())
			{
				ChaseBurstTimeRemaining = ChaseBurstDuration;
			}
		}
	}
}

float AStalkerEnemy::GetLocomotionBaseSpeed() const
{
	const float Base = Super::GetLocomotionBaseSpeed();
	if (ChaseBurstTimeRemaining > KINDA_SMALL_NUMBER &&
		(EnemyState == EEnemyAIState::Chase || EnemyState == EEnemyAIState::Attack))
	{
		return Base * ChaseBurstSpeedMultiplier;
	}
	return Base;
}

bool AStalkerEnemy::ShouldTriggerChaseBurstFromBehind() const
{
	if (!IsValid(TargetActor))
	{
		return false;
	}

	const FVector PlayerLoc = TargetActor->GetActorLocation();
	const FVector ToEnemy = (GetActorLocation() - PlayerLoc).GetSafeNormal();
	if (ToEnemy.IsNearlyZero())
	{
		return false;
	}

	const FVector PlayerForward = TargetActor->GetActorForwardVector().GetSafeNormal();
	const float Dot = FVector::DotProduct(PlayerForward, ToEnemy);
	return Dot < BehindPlayerDotThreshold;
}

void AStalkerEnemy::UpdateFlashlightReveal()
{
	if (!bFlashlightStealth)
	{
		ApplyFlashlightReveal(true);
		return;
	}

	if (!IsAlive())
	{
		ApplyFlashlightReveal(true);
		return;
	}

	const bool bInCone = ComputeInPlayerFlashlightCone();
	ApplyFlashlightReveal(bInCone);
}

bool AStalkerEnemy::ComputeInPlayerFlashlightCone() const
{
	if (!IsValid(TargetActor))
	{
		return true;
	}

	const AOblivioCharacter* Player = Cast<AOblivioCharacter>(TargetActor);
	if (!Player)
	{
		return true;
	}

	const USpotLightComponent* Spot = Player->FlashlightComponent;
	if (!Spot)
	{
		return false;
	}

	// 캐릭터 쪽에서 조명 연출용으로 Spot을 숨길 수 있음(BeginPlay SetVisibility false 등).
	// 손전등 “논리적 ON”은 아래 bIsFlashlightOn / Battery 로만 본다.

	if (!Player->bIsFlashlightOn || Player->Battery <= 0.0f)
	{
		return false;
	}

	const FVector TestPoint = GetActorLocation() + FlashlightTestPointOffset;
	const FVector Origin = Spot->GetComponentLocation();
	const FVector Dir = Spot->GetForwardVector().GetSafeNormal();
	const FVector ToPoint = TestPoint - Origin;
	const float Dist = ToPoint.Size();
	if (Dist < KINDA_SMALL_NUMBER)
	{
		return true;
	}

	const float MaxDist = Spot->AttenuationRadius + FlashlightConeRadiusSlack;
	if (Dist > MaxDist)
	{
		return false;
	}

	const FVector ToNorm = ToPoint / Dist;
	const float HalfOuterDeg = Spot->OuterConeAngle * 0.5f;
	const float CosCone = FMath::Cos(FMath::DegreesToRadians(HalfOuterDeg));
	return FVector::DotProduct(Dir, ToNorm) >= CosCone - KINDA_SMALL_NUMBER;
}

float AStalkerEnemy::GetStealthAudioEnvelopeTarget() const
{
	if (EnemyState == EEnemyAIState::Dead)
	{
		return 0.0f;
	}
	if (!bFlashlightStealth)
	{
		return 1.0f;
	}
	return ComputeInPlayerFlashlightCone() ? 1.0f : 0.0f;
}

void AStalkerEnemy::UpdateStalkerStealthAudioEnvelope(float DeltaSeconds)
{
	if (!StalkerAudioComponent || !StalkerRevealLoopSound)
	{
		return;
	}

	const float Target = GetStealthAudioEnvelopeTarget();
	const float AttackRate = FMath::Max(StalkerAudioAttackSeconds, 0.01f);
	const float ReleaseRate = FMath::Max(StalkerAudioReleaseSeconds, 0.01f);
	const float Speed = 1.0f / ((Target > StealthAudioEnvelope + KINDA_SMALL_NUMBER) ? AttackRate : ReleaseRate);

	StealthAudioEnvelope = FMath::FInterpConstantTo(StealthAudioEnvelope, Target, DeltaSeconds, Speed);
	StealthAudioEnvelope = FMath::Clamp(StealthAudioEnvelope, 0.0f, 1.0f);

	const float OutVol = StealthAudioEnvelope * FMath::Max(0.0f, EnemySoundVolumeMultiplier);
	StalkerAudioComponent->SetVolumeMultiplier(OutVol);

	if (OutVol > 0.02f)
	{
		if (!StalkerAudioComponent->IsPlaying())
		{
			StalkerAudioComponent->Play();
		}
	}
	else if (StalkerAudioComponent->IsPlaying())
	{
		StalkerAudioComponent->Stop();
	}
}

void AStalkerEnemy::ApplyFlashlightReveal(bool bRevealed)
{
	const bool bChanged = (bRevealedByFlashlight != bRevealed);
	bRevealedByFlashlight = bRevealed;

	if (USkeletalMeshComponent* SkelMesh = GetMesh())
	{
		SkelMesh->SetVisibility(bRevealed, true);
		SkelMesh->SetCastShadow(bRevealed);
	}

	if (bChanged)
	{
		OnFlashlightRevealChanged(bRevealed);
	}
}

void AStalkerEnemy::OnFlashlightRevealChanged_Implementation(bool bRevealed)
{
	(void)bRevealed;
}

void AStalkerEnemy::ApplyEnemySoundVolumes()
{
	Super::ApplyEnemySoundVolumes();

	if (!StalkerAudioComponent)
	{
		return;
	}

	const float OutVol = StealthAudioEnvelope * FMath::Max(0.0f, EnemySoundVolumeMultiplier);
	StalkerAudioComponent->SetVolumeMultiplier(OutVol);
}
