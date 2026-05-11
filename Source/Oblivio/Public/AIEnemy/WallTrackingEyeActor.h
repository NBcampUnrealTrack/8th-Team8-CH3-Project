#pragma once

// =============================================================================
// AWallTrackingEyeActor — 벽에 배치하는 눈. LookPivot을 돌려 동공(애로우가 가리키는 축)이 플레이어를 본다.
// 에디터에서 LookDirectionArrow 로컬 회전으로 "동공 바깥 방향"을 맞춘 뒤, EyeMesh만 메시에 맞게 상대 배치.
// =============================================================================

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WallTrackingEyeActor.generated.h"

class UStaticMeshComponent;
class USceneComponent;
class UArrowComponent;
class USpotLightComponent;

UCLASS(Blueprintable)
class OBLIVIO_API AWallTrackingEyeActor : public AActor
{
	GENERATED_BODY()

public:
	AWallTrackingEyeActor();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> RootAnchor;

	/** 회전은 이 피벗만 적용. 위치는 동공 중심(또는 회전 기준점)에 두는 것을 권장. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> LookPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> EyeMesh;

	/** 에디터에서 동공 바깥 방향으로 돌려 두면, 런타임에 이 축이 플레이어를 향하도록 LookPivot이 회전한다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UArrowComponent> LookDirectionArrow;

	/** 애로우(+X) 축 방향으로 붉은 광원을 쏜다. 위치는 애로우 루트(필요 시 Eye Glow Forward Offset으로 조정). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USpotLightComponent> EyeGlowSpot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eye Glow")
	bool bEnableEyeGlow = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eye Glow")
	FLinearColor EyeGlowColor = FLinearColor(1.f, 0.12f, 0.08f, 1.f);

	/** UE5 스팟 기본 단위에 맞춘 밝기 에디터에서 튜닝 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eye Glow", meta = (ClampMin = "0"))
	float EyeGlowIntensity = 25000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eye Glow", meta = (ClampMin = "100"))
	float EyeGlowRange = 4000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eye Glow", meta = (ClampMin = "1", ClampMax = "80"))
	float EyeGlowInnerConeDegrees = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eye Glow", meta = (ClampMin = "1", ClampMax = "85"))
	float EyeGlowOuterConeDegrees = 22.f;

	/** 동공 앞으로 빼고 싶을 때 애로우 로컬 +X(cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eye Glow")
	float EyeGlowForwardOffset = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracking")
	bool bSmoothRotation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracking", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float RotationInterpSpeed = 12.f;

	/** 0 이하면 추적 거리 제한 없음. 양수면 cm 단위로 이 거리 밖이면 회전하지 않음. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracking")
	float MaxTrackingDistance = 0.f;

	/** LookPivot 최종 회전에 곱해질 추가 보정(드물게 메시·축 미세 조정용) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracking")
	FRotator MeshRotationOffset = FRotator::ZeroRotator;

	/** 참이면 월드 업 기준으로 롤만 제한(오일러 롤 0 금지: 시선 붕괴). 끄면 자유 회전. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracking")
	bool bZeroRoll = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracking", meta = (ClampMin = "0", UIMin = "0"))
	int32 PlayerIndex = 0;

	/** 켜면 Output Log에 추적 상태를 남김(간격은 Debug Log Interval). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracking|Debug")
	bool bDebugLog = false;

	/** bDebugLog일 때 로그 최소 간격(초). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracking|Debug", meta = (EditCondition = "bDebugLog", ClampMin = "0.05", UIMin = "0.05"))
	float DebugLogInterval = 0.25f;

private:
	void UpdateLookAt(float DeltaSeconds);
	bool ShouldEmitDebugLog();
	void EmitDebugLog(const TCHAR* Reason, const FString& Message);
	void ApplyEyeGlowSettings();

	float LastDebugLogTime = -1000.f;
};
