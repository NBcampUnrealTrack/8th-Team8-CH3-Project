// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FloodLevelActor.generated.h"

UCLASS()
class OBLIVIO_API AFloodLevelActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AFloodLevelActor();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	/** 홍수 이벤트를 시작시키는 함수 */
	UFUNCTION(BlueprintCallable, Category = "Flood")
	void StartFloodEvent();

	/** 현재 수위 값을 반환하는 Getter */
	float GetCurrentFloodHeight() const { return CurrentFloodHeight; }
	/** 홍수 이벤트 및 관련 사운드를 중단하는 함수 */
	UFUNCTION(BlueprintCallable, Category = "Flood")
	void StopFloodEffects();
protected:
	/** 수면을 시각적으로 보여줄 스태틱 메쉬 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UStaticMeshComponent* WaterMesh;

	/** 홍수 시작 시 목표로 하는 최종 수위 높이 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood | Settings")
	float TargetFloodHeight = 150.0f;

	/** 수위가 차오르는 속도 (초당 유닛) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood | Settings")
	float FloodRiseSpeed = 2.5f;

	/** 에디터에서 선택할 타임어택 위젯 클래스 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|UI")
	TSubclassOf<class UUserWidget> TimerWidgetClass;

	/** 홍수 시작 시 재생될 효과음 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|Sound")
	class USoundBase* FloodStartSound;

	/** 여자 울음소리 루프 사운드 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|Sound")
	class USoundBase* WomanCryingLoop;

	/** 사운드 제어를 위한 컴포넌트 */
	UPROPERTY()
	class UAudioComponent* CryingAudioComp;

	UPROPERTY()
	class UUserWidget* ActiveTimerWidget;

private:
	/** 홍수 진행 여부 플래그 */
	bool bIsFlooding = false;

	/** 현재 수위 값 */
	float CurrentFloodHeight;

	/** 월드에 존재하는 플레이어 참조 (캐싱용) */
	UPROPERTY()
	class AOblivioCharacter* PlayerRef;
};
