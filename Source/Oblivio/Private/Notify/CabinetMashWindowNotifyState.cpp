#include "Notify/CabinetMashWindowNotifyState.h"

#include "AIEnemy/CabinetEnemy.h"

FString UCabinetMashWindowNotifyState::GetNotifyName_Implementation() const
{
	return FString::Printf(TEXT("Cabinet Mash (%d presses)"), RequiredPressCount);
}

void UCabinetMashWindowNotifyState::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!MeshComp)
	{
		return;
	}

	if (ACabinetEnemy* CabinetEnemy = Cast<ACabinetEnemy>(MeshComp->GetOwner()))
	{
		CabinetEnemy->BeginMashWindow(RequiredPressCount);
	}
}

void UCabinetMashWindowNotifyState::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	if (ACabinetEnemy* CabinetEnemy = Cast<ACabinetEnemy>(MeshComp->GetOwner()))
	{
		CabinetEnemy->EndMashWindow();
	}
}
