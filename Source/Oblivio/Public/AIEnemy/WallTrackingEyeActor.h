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
class USoundBase;
class USoundAttenuation;
class UAudioComponent;

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

	/** 회전 사운드 단일 재생용(중첩 방지). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UAudioComponent> EyeRotateAudio;

	/**
	 * true면 평소 추적·안광 끔 → MementoEye 메멘토 획득 후에만 동작. GameInstance bMementoEyeCollected 연동.
	 * 비워 두면 아래 태그·층 번호로 켤 수 있음.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eye|Memento Eye")
	bool bIdleUntilMementoEyeItem = false;

	/**
	 * 0이면 미사용. 양수면 GameInstance CurrentFloor 와 같을 때만 메멘토 게이트 적용.
	 * (7층 맵을 바로 PIE 하면 CurrentFloor 가 9인 경우가 많아 불일치할 수 있음 → 태그 MementoGatedEye 또는 bIdleUntilMementoEyeItem 권장.)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eye|Memento Eye", meta = (ClampMin = "0"))
	int32 MementoGateApplyOnFloor = 0;

	/** 끄면 붉은 스팟 비활성(게이트 해제 후 이 값이 실제 안광 여부를 결정). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eye Glow")
	bool bEnableEyeGlow = false;

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

	/** 켜면 추적 실패 시에만 Output Log에 이유 출력(정상 추적일 때는 찍지 않음). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracking|Debug")
	bool bDebugLog = false;

	/** bDebugLog일 때 로그 최소 간격(초). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracking|Debug", meta = (EditCondition = "bDebugLog", ClampMin = "0.05", UIMin = "0.05"))
	float DebugLogInterval = 0.25f;

	/** LookPivot 회전이 이번 틱에 적용된 뒤 호출된다. 블루프린트에서 오버라이드해 사운드 등 처리. DeltaDegrees는 직전 쿼터니언 대비 각 변화량(도). */
	UFUNCTION(BlueprintNativeEvent, Category = "WallEye|Audio")
	void OnEyeRotationApplied(float DeltaDegreesThisFrame, float DeltaSeconds);
	virtual void OnEyeRotationApplied_Implementation(float DeltaDegreesThisFrame, float DeltaSeconds);

	/** 켜져 있을 때만 C++ 기본 회전 사운드를 재생한다. 맵에 둔 인스턴스 중 소리 낼 액터만 체크할 것(블루프린트 클래스 디폴트는 끔). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallEye|Audio")
	bool bPlayEyeRotateSound = false;

	/** 기본 재생용(비어 있으면 C++는 조용히 둠). 오버라이드만 쓸 때는 비워두면 된다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallEye|Audio", meta = (EditCondition = "bPlayEyeRotateSound"))
	TObjectPtr<USoundBase> EyeRotateSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallEye|Audio", meta = (ClampMin = "0"))
	float EyeRotateSoundMinDeltaDegrees = 0.12f;

	/** 한 번 재생 후 다시 허용할 최소 간격(초). 0이면 쿨다운 없음. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallEye|Audio", meta = (ClampMin = "0"))
	float EyeRotateSoundCooldown = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallEye|Audio", meta = (ClampMin = "0", ClampMax = "4"))
	float EyeRotateSoundVolumeMultiplier = 1.f;

	/** 0이면 거리 제한 없음. 플레이와의 거리(cm)가 이 값보다 크면 C++ 기본 재생 생략 — 눈이 많을 때 필수. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallEye|Audio", meta = (ClampMin = "0"))
	float EyeRotateSoundMaxDistanceFromPlayer = 1000.f;

	/** 1이면 매번 시도, 낮출수록 무작위로 건너뛰어 동시 재생 밀도 완화 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallEye|Audio", meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float EyeRotateSoundPlayProbability = 0.35f;

	/** 피치 배율 ±지터(예: 0.05 → 0.95~1.05). 0이면 고정. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallEye|Audio", meta = (ClampMin = "0", ClampMax = "0.4"))
	float EyeRotateSoundPitchJitter = 0.06f;

	/** 비어 있어도 동작. 에셋으로 감쇠 거리·곡선 지정 시 밀집 구간에 유리 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallEye|Audio")
	TObjectPtr<USoundAttenuation> EyeRotateSoundAttenuation;

private:
	void UpdateLookAt(float DeltaSeconds);
	bool ShouldEmitDebugLog();
	void EmitDebugLog(const TCHAR* Reason, const FString& Message);
	void ApplyEyeGlowSettings();

	/** bIdleUntilMementoEyeItem 일 때 GI 플래그. */
	bool QueryMementoEyeUnlockedFromGameInstance() const;
	/** 체크박스·태그 MementoGatedEye·MementoGateApplyOnFloor 중 하나라도 만족하면 메멘토까지 눈알 정지. */
	bool UsesMementoEyeGate() const;
	bool GetEffectiveEyeGlowVisible() const;
	bool ShouldRunPlayerTracking() const;

	float LastDebugLogTime = -1000.f;

	/** OnEyeRotationApplied용 직전 LookPivot 회전 */
	FQuat LastLookPivotQuatForAudio = FQuat::Identity;
	float LastEyeRotateSoundWorldTime = -1000.f;
	bool bEyeAudioBaselineSet = false;
};
