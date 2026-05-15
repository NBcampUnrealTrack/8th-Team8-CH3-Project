#include "OblivioComponents/SoundPropagationComponent.h"
#include "AIEnemy/EnemyBase.h"

#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/EngineTypes.h"

USoundPropagationComponent::USoundPropagationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	StimulusType = EEnemyStimulusType::Noise;
	Radius = 700;
}

void USoundPropagationComponent::PropagateSound(float VolumeMultiplier)
{
	UE_LOG(LogTemp, Warning, TEXT("Stimulusing nearby enemies..."));
	FVector SoundLocation = GetOwner()->GetActorLocation();

	float FinalRadius = Radius * VolumeMultiplier;

	// 소리 전파 범위 표시
	if (bShowDebugSphere)
	{
		DrawDebugCircle(GetWorld(), SoundLocation + FVector::UpVector * 20, FinalRadius, 16, FColor::Cyan, false, 0.5f, 0, 2.0f, FVector::ForwardVector, FVector::RightVector, false);
	}
	//주변 적 파악 (ECC_Pawn + ECC_WorldDynamic 명시 — 빈 배열이면 아무것도 감지 안 됨)
	TArray<AActor*> OverlapActors;
	TArray<TEnumAsByte<EObjectTypeQuery>> EnemyObjectTypes;
	EnemyObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
	EnemyObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));

	UKismetSystemLibrary::SphereOverlapActors(
		GetWorld(),
		SoundLocation,
		FinalRadius,
		EnemyObjectTypes,
		AEnemyBase::StaticClass(),
		TArray{ GetOwner() },
		OverlapActors);
	UE_LOG(LogTemp, Log, TEXT("Sound propagated to %d enemies"), OverlapActors.Num());
	if (OverlapActors.IsEmpty()) return;

	//있는 적에게 소리 자극 전달, 자극 타입은 에디터에서 변경 가능
	for (AActor* Result : OverlapActors)
	{
		if (AEnemyBase* Enemy = Cast<AEnemyBase>(Result))
		{
			Enemy->ReportStimulus(SoundLocation, StimulusType);
		}
	}
}
