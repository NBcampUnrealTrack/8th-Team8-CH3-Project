#include "Notify/CabinetGrabApproachNotifyState.h"

#include "AIEnemy/CabinetEnemy.h"

FString UCabinetGrabApproachNotifyState::GetNotifyName_Implementation() const
{
	return TEXT("Cabinet Grab Approach");
}

void UCabinetGrabApproachNotifyState::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!MeshComp)
	{
		return;
	}

	if (ACabinetEnemy* CabinetEnemy = Cast<ACabinetEnemy>(MeshComp->GetOwner()))
	{
		CabinetEnemy->BeginGrabApproach(TotalDuration);
	}
}

void UCabinetGrabApproachNotifyState::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);
	// 접근 이동은 ACabinetEnemy::Tick 에서 처리 (SingleNode·NotifyTick 중복 방지).
}

void UCabinetGrabApproachNotifyState::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	if (ACabinetEnemy* CabinetEnemy = Cast<ACabinetEnemy>(MeshComp->GetOwner()))
	{
		CabinetEnemy->EndGrabApproach();
	}
}
