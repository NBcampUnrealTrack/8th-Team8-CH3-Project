#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RoomCeiling.generated.h"

class UBoxComponent;
class UMaterialInstanceDynamic;
class UPrimitiveComponent;
class USceneComponent;
class UStaticMeshComponent;

/**
 * 방 단위 천장 액터.
 * SceneRoot(루트) 위치 = 배치 핸들. CeilingMesh 는 (0,0,0), RoomTrigger 는 바닥 쪽 오프셋.
 */
UCLASS(Blueprintable)
class OBLIVIO_API ARoomCeiling : public AActor
{
	GENERATED_BODY()

public:
	ARoomCeiling();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;

	UFUNCTION(BlueprintCallable, Category = "RoomCeiling")
	void RefreshCeilingVisibility();

	/** CeilingMesh(및 Extra) XY에 RoomTrigger 크기·중심을 맞춥니다. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "RoomCeiling")
	void FitRoomTriggerToCeilingMesh();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RoomCeiling")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RoomCeiling")
	TObjectPtr<UStaticMeshComponent> CeilingMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RoomCeiling")
	TObjectPtr<UBoxComponent> RoomTrigger;

	/** true면 RoomTriggerHalfExtent.X/Y·중심 XY를 천장 메시에 맞춤(Z·RoomTriggerRelativeLocation.Z 는 유지). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RoomCeiling")
	bool bAutoFitTriggerXYToCeilingMesh = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RoomCeiling")
	FVector RoomTriggerRelativeLocation = FVector(0.f, 0.f, -280.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RoomCeiling", meta = (ClampMin = "1.0"))
	FVector RoomTriggerHalfExtent = FVector(200.f, 200.f, 120.f);

	/** XY 판정 시 가장자리에서 줄일 반경(복도·문 오탐 방지). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RoomCeiling", meta = (ClampMin = "0.0"))
	float RoomTriggerXYInset = 0.f;

	/** true면 Z 무시(XY만). 복도가 같은 XY 아래에 있으면 오탐 — 기본 false 권장. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RoomCeiling")
	bool bIgnoreZForRoomTest = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RoomCeiling", meta = (UseComponentPicker))
	TArray<TObjectPtr<UStaticMeshComponent>> ExtraCeilingMeshes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RoomCeiling")
	TArray<TObjectPtr<AActor>> ExternalCeilingActors;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RoomCeiling")
	bool bAutoCollectAdditionalStaticMeshes = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RoomCeiling")
	bool bOnlyAffectLocallyControlledPlayer = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RoomCeiling")
	bool bUsePawnFeetForRoomTest = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RoomCeiling|Fade")
	bool bUseOpacityFade = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RoomCeiling|Fade",
		meta = (ClampMin = "0.01", EditCondition = "bUseOpacityFade"))
	float FadeDuration = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RoomCeiling|Fade",
		meta = (EditCondition = "bUseOpacityFade"))
	FName OpacityMaterialParameterName = TEXT("Opacity");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RoomCeiling|Fade",
		meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bUseOpacityFade"))
	float HiddenOpacityThreshold = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RoomCeiling|Debug")
	bool bDebugDrawRoomTest = false;

	void ApplyRoomTriggerLayout();
	void FitRoomTriggerXYToCeilingMesh();
	void ApplyDebugVisualization(bool bPlayerInside);
	void BuildCeilingPrimitiveList();
	void UpdatePlayerInsideState();
	void RestoreCeilingForEditor();
	void ApplyInstantCeilingHidden(bool bHideCeiling);
	void ApplyOpacityToCeilings(float Opacity);
	void EnsureFadeMaterialInstances();
	void UpdateOpacityFade();
	void SetOpacityFadeTarget(float InTargetOpacity);
	void DrawRoomDebug(const APawn* TrackedPawn, bool bPlayerInside) const;

	FVector GetRoomTestLocationForPawn(const APawn* Pawn) const;
	bool IsPawnInsideRoomTrigger(const APawn* Pawn) const;
	bool ShouldTrackPawn(const APawn* Pawn) const;
	APawn* FindTrackedPlayerPawn() const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPrimitiveComponent>> ResolvedCeilingPrimitives;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> CeilingFadeMIDs;

	UPROPERTY(Transient)
	bool bAnyTrackedPlayerInside = false;

	UPROPERTY(Transient)
	float CurrentCeilingOpacity = 1.f;

	UPROPERTY(Transient)
	float TargetCeilingOpacity = 1.f;

	UPROPERTY(Transient)
	float PlayerInsidePollAccumSec = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RoomCeiling", meta = (ClampMin = "0.02"))
	float PlayerInsidePollIntervalSec = 0.05f;

	FTimerHandle OpacityFadeTimerHandle;
};
