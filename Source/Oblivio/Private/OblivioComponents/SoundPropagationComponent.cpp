#include "OblivioComponents/SoundPropagationComponent.h"
#include "AIEnemy/EnemyBase.h"

#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "Kismet/KismetSystemLibrary.h"

USoundPropagationComponent::USoundPropagationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	StimulusType = EEnemyStimulusType::Noise;
	Radius = 700;
}

void USoundPropagationComponent::PropagateSound()
{
	FVector SoundLocation = GetOwner()->GetActorLocation();

	// 소리 전파 범위 표시
	if (bShowDebugSphere)
	{
		DrawDebugCircle(GetWorld(), SoundLocation, Radius, 16, FColor::Cyan, false, 0.5f, 0, 2.0f, FVector::ForwardVector,FVector::RightVector, false);
	}
	//주변 적 파악
	TArray<AActor*> OverlapActors;

	UKismetSystemLibrary::SphereOverlapActors(
		GetWorld(),
		SoundLocation,
		Radius,
		TArray<TEnumAsByte<EObjectTypeQuery>>(),
		AEnemyBase::StaticClass(),
		TArray{ GetOwner() },
		OverlapActors);
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
