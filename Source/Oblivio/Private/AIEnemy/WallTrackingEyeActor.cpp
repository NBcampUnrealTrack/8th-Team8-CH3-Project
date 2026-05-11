#include "AIEnemy/WallTrackingEyeActor.h"

#include "Components/ArrowComponent.h"
#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogWallTrackingEye, Log, All);

AWallTrackingEyeActor::AWallTrackingEyeActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	RootAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("RootAnchor"));
	RootComponent = RootAnchor;

	LookPivot = CreateDefaultSubobject<USceneComponent>(TEXT("LookPivot"));
	LookPivot->SetupAttachment(RootAnchor);

	EyeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EyeMesh"));
	EyeMesh->SetupAttachment(LookPivot);
	EyeMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	EyeMesh->SetCollisionResponseToAllChannels(ECR_Ignore);

	LookDirectionArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("LookDirectionArrow"));
	LookDirectionArrow->SetupAttachment(LookPivot);
	LookDirectionArrow->bIsEditorOnly = false;
	LookDirectionArrow->bHiddenInGame = true;
	LookDirectionArrow->SetArrowColor(FLinearColor(1.f, 0.85f, 0.1f, 1.f));
	LookDirectionArrow->SetArrowLength(35.f);

	EyeGlowSpot = CreateDefaultSubobject<USpotLightComponent>(TEXT("EyeGlowSpot"));
	EyeGlowSpot->SetupAttachment(LookDirectionArrow);
	EyeGlowSpot->SetMobility(EComponentMobility::Movable);
	EyeGlowSpot->SetCastShadows(false);
	EyeGlowSpot->bUseInverseSquaredFalloff = false;

	EyeRotateAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("EyeRotateAudio"));
	EyeRotateAudio->SetupAttachment(LookPivot);
	EyeRotateAudio->bAutoActivate = false;
	EyeRotateAudio->bAutoDestroy = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> EyeAsset(
		TEXT("/Game/Fab/low_poly_eye/low_poly_eye.low_poly_eye"));
	if (EyeAsset.Succeeded())
	{
		EyeMesh->SetStaticMesh(EyeAsset.Object);
	}

	ApplyEyeGlowSettings();
}

void AWallTrackingEyeActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyEyeGlowSettings();
}

void AWallTrackingEyeActor::ApplyEyeGlowSettings()
{
	if (!IsValid(EyeGlowSpot))
	{
		return;
	}

	EyeGlowSpot->SetVisibility(bEnableEyeGlow);

	const float Inner = FMath::Min(EyeGlowInnerConeDegrees, EyeGlowOuterConeDegrees);
	const float Outer = FMath::Max(EyeGlowInnerConeDegrees, EyeGlowOuterConeDegrees);
	EyeGlowSpot->SetIntensity(FMath::Max(0.f, EyeGlowIntensity));
	EyeGlowSpot->SetLightColor(EyeGlowColor);
	EyeGlowSpot->SetAttenuationRadius(FMath::Max(100.f, EyeGlowRange));
	EyeGlowSpot->SetInnerConeAngle(Inner);
	EyeGlowSpot->SetOuterConeAngle(FMath::Max(Outer, Inner + 1.f));
	EyeGlowSpot->SetRelativeLocation(FVector(EyeGlowForwardOffset, 0.f, 0.f));

	// 스팟이 애로우 +X(붉은축)로 나가도록 — 추가 회전 없음
}

void AWallTrackingEyeActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	SetActorTickEnabled(true);
}

void AWallTrackingEyeActor::BeginPlay()
{
	Super::BeginPlay();
	SetActorTickEnabled(true);

	if (IsValid(LookPivot))
	{
		LastLookPivotQuatForAudio = LookPivot->GetComponentQuat();
		bEyeAudioBaselineSet = true;
	}
}

bool AWallTrackingEyeActor::ShouldEmitDebugLog()
{
	if (!bDebugLog)
	{
		return false;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	const float Now = World->GetTimeSeconds();
	if (Now - LastDebugLogTime < DebugLogInterval)
	{
		return false;
	}
	LastDebugLogTime = Now;
	return true;
}

void AWallTrackingEyeActor::EmitDebugLog(const TCHAR* Reason, const FString& Message)
{
	if (!ShouldEmitDebugLog())
	{
		return;
	}
	UE_LOG(LogWallTrackingEye, Log, TEXT("[%s] %s — %s"), *GetName(), Reason, *Message);
}

void AWallTrackingEyeActor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateLookAt(DeltaSeconds);
}

static APawn* ResolvePlayerPawn(UWorld* World, const int32 PlayerIndex)
{
	if (!World)
	{
		return nullptr;
	}
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, PlayerIndex))
	{
		if (APawn* P = PC->GetPawn())
		{
			return P;
		}
	}
	if (APawn* P = UGameplayStatics::GetPlayerCharacter(World, PlayerIndex))
	{
		return P;
	}
	return UGameplayStatics::GetPlayerPawn(World, PlayerIndex);
}

static FQuat MakeQuatPivotXAlignDirUpright(const FVector& DirToPlayer)
{
	const FVector X = DirToPlayer.GetSafeNormal();
	FVector ZRef = FVector::UpVector;
	if (FMath::Abs(FVector::DotProduct(X, ZRef)) > 0.99f)
	{
		ZRef = FVector::ForwardVector;
	}
	return FRotationMatrix::MakeFromXZ(X, ZRef).ToQuat();
}

void AWallTrackingEyeActor::UpdateLookAt(const float DeltaSeconds)
{
	if (!IsValid(LookPivot) || !IsValid(LookDirectionArrow))
	{
		if (ShouldEmitDebugLog())
		{
			UE_LOG(LogWallTrackingEye, Warning,
			       TEXT("[%s] Skip — LookPivot or LookDirectionArrow invalid"), *GetName());
		}
		return;
	}

	APawn* PlayerPawn = ResolvePlayerPawn(GetWorld(), PlayerIndex);
	if (!IsValid(PlayerPawn))
	{
		EmitDebugLog(TEXT("Skip"),
		             FString::Printf(TEXT("no pawn idx=%d (컨트롤러·폰 스폰·빙의 확인)."), PlayerIndex));
		return;
	}

	const FVector PivotLocation = LookPivot->GetComponentLocation();
	const FVector TargetLocation = PlayerPawn->GetActorLocation();

	if (MaxTrackingDistance > 0.f)
	{
		const float DistSq = FVector::DistSquared(PivotLocation, TargetLocation);
		if (DistSq > FMath::Square(MaxTrackingDistance))
		{
			const float Dist = FMath::Sqrt(DistSq);
			EmitDebugLog(TEXT("Skip"),
			             FString::Printf(TEXT("out of range: dist=%.1f > max=%.1f"), Dist, MaxTrackingDistance));
			return;
		}
	}

	const FVector DirToPlayer = (TargetLocation - PivotLocation).GetSafeNormal();
	if (DirToPlayer.IsNearlyZero())
	{
		EmitDebugLog(TEXT("Skip"), FString(TEXT("DirToPlayer ~ 0 (pivot와 타겟 위치가 거의 같음)")));
		return;
	}

	const FVector ArrowAxisLocal = LookDirectionArrow->GetRelativeRotation()
		.RotateVector(FVector::ForwardVector);
	const FVector ArrowAxisNorm = ArrowAxisLocal.GetSafeNormal();
	if (ArrowAxisNorm.IsNearlyZero())
	{
		EmitDebugLog(TEXT("Skip"),
		             FString(TEXT("arrow axis degenerate — LookDirectionArrow 상대 회전 확인")));
		return;
	}

	const FQuat QPivot = LookPivot->GetComponentQuat();
	const FVector CurrentArrowFwd = QPivot.RotateVector(ArrowAxisNorm);

	FQuat QTarget = QPivot;
	if (bZeroRoll)
	{
		// 애로우 축(피벗 로컬)이 월드 Dir이 되도록 하되, 회전 행렬로 롤만 억제 (오일러 Roll=0 사용 금지)
		const FQuat QWorldFrame = MakeQuatPivotXAlignDirUpright(DirToPlayer);
		const FQuat QFromXToLocal = FQuat::FindBetweenNormals(FVector::ForwardVector, ArrowAxisNorm);
		QTarget = QWorldFrame * QFromXToLocal.Inverse();
	}
	else
	{
		const FQuat QDelta = FQuat::FindBetweenNormals(CurrentArrowFwd, DirToPlayer);
		QTarget = QDelta * QPivot;
	}

	if (!MeshRotationOffset.IsNearlyZero())
	{
		QTarget *= MeshRotationOffset.Quaternion();
	}

	const bool bCanInterp = bSmoothRotation && RotationInterpSpeed > KINDA_SMALL_NUMBER;
	if (bCanInterp)
	{
		const float Alpha = FMath::Clamp(DeltaSeconds * RotationInterpSpeed, 0.f, 1.f);
		const FQuat QSmoothed = FQuat::Slerp(QPivot, QTarget, Alpha);
		LookPivot->SetWorldRotation(QSmoothed);
	}
	else
	{
		LookPivot->SetWorldRotation(QTarget);
	}

	const FQuat QAfter = LookPivot->GetComponentQuat();
	if (bEyeAudioBaselineSet)
	{
		const float DeltaDeg = FMath::RadiansToDegrees(LastLookPivotQuatForAudio.AngularDistance(QAfter));
		OnEyeRotationApplied(DeltaDeg, DeltaSeconds);
	}
	LastLookPivotQuatForAudio = QAfter;
	bEyeAudioBaselineSet = true;
}

void AWallTrackingEyeActor::OnEyeRotationApplied_Implementation(float DeltaDegreesThisFrame, float DeltaSeconds)
{
	if (!bPlayEyeRotateSound)
	{
		return;
	}
	if (!IsValid(EyeRotateSound))
	{
		return;
	}
	if (DeltaDegreesThisFrame < EyeRotateSoundMinDeltaDegrees)
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!IsValid(World) || !IsValid(LookPivot))
	{
		return;
	}
	if (EyeRotateSoundCooldown > KINDA_SMALL_NUMBER
	    && World->GetTimeSeconds() - LastEyeRotateSoundWorldTime < EyeRotateSoundCooldown)
	{
		return;
	}

	if (EyeRotateSoundMaxDistanceFromPlayer > KINDA_SMALL_NUMBER && IsValid(LookPivot))
	{
		if (APawn* ListenerPawn = ResolvePlayerPawn(World, PlayerIndex))
		{
			const float D = FVector::Dist(LookPivot->GetComponentLocation(),
			                              ListenerPawn->GetActorLocation());
			if (D > EyeRotateSoundMaxDistanceFromPlayer)
			{
				return;
			}
		}
	}

	const float Prob = FMath::Clamp(EyeRotateSoundPlayProbability, 0.f, 1.f);
	if (Prob < 1.f - KINDA_SMALL_NUMBER && FMath::FRand() > Prob)
	{
		return;
	}

	float PitchMul = 1.f;
	if (EyeRotateSoundPitchJitter > KINDA_SMALL_NUMBER)
	{
		PitchMul = FMath::RandRange(1.f - EyeRotateSoundPitchJitter, 1.f + EyeRotateSoundPitchJitter);
	}

	LastEyeRotateSoundWorldTime = World->GetTimeSeconds();

	if (!IsValid(EyeRotateAudio))
	{
		return;
	}

	EyeRotateAudio->Stop();
	EyeRotateAudio->SetSound(EyeRotateSound);
	EyeRotateAudio->SetVolumeMultiplier(EyeRotateSoundVolumeMultiplier);
	EyeRotateAudio->SetPitchMultiplier(PitchMul);
	EyeRotateAudio->SetAttenuationSettings(IsValid(EyeRotateSoundAttenuation) ? EyeRotateSoundAttenuation.Get()
	                                                                           : nullptr);
	EyeRotateAudio->Play();
}
