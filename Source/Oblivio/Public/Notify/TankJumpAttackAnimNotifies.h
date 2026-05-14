#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "TankJumpAttackAnimNotifies.generated.h"

/** 애님 30fps · 프레임 15 — 이륙 순간 타겟 위치를 잠그고 호흡궤 도약을 시작합니다. */
UCLASS(meta = (DisplayName = "Tank Jump Lift Off (CPP)"))
class OBLIVIO_API UTankJumpLiftOffNotify : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual FString GetNotifyName_Implementation() const override { return TEXT("TankJumpLiftOff"); }

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};

/** 애님 프레임 33 — 좁은 착지 피해 + 피해야 할 가로 확장 충격(점프 높이로 회피). */
UCLASS(meta = (DisplayName = "Tank Jump Landing Impact (CPP)"))
class OBLIVIO_API UTankJumpLandingNotify : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual FString GetNotifyName_Implementation() const override { return TEXT("TankJumpLanding"); }

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};

/** 애님 프레임 60 — 몽타주·FSM 정리 후 Chase/추격 복귀. */
UCLASS(meta = (DisplayName = "Tank Jump Montage Finished (CPP)"))
class OBLIVIO_API UTankJumpFinishedNotify : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual FString GetNotifyName_Implementation() const override { return TEXT("TankJumpFinished"); }

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
