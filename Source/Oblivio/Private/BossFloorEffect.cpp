// Fill out your copyright notice in the Description page of Project Settings.


#include "BossFloorEffect.h"
#include "AIEnemy/LuxeaterEnemy.h"
#include "Kismet/GameplayStatics.h"

// Sets default values
ABossFloorEffect::ABossFloorEffect()
{
	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>("MeshComp");
	RootComponent = MeshComp;
}

// Called when the game starts or when spawned
void ABossFloorEffect::BeginPlay()
{
	Super::BeginPlay();

	//보스와 함수 바인딩
	TArray<AActor*> Bosses;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("Boss"), Bosses);
	UE_LOG(LogTemp, Warning, TEXT("Found %d boss actors"), Bosses.Num());
	if (Bosses.Num() > 0) {
		for(AActor* Boss : Bosses)
		if (ALuxeaterEnemy* Target = Cast<ALuxeaterEnemy>(Boss)) {
			UE_LOG(LogTemp, Warning, TEXT("Binding floor effect"));
			Target->OnEnteredPhaseTwo.AddDynamic(this, &ABossFloorEffect::StartEffect);
			Target->OnEndPlay.AddDynamic(this, &ABossFloorEffect::ClearEffect);
		}
	}
	
	//지정된 머티리얼 있으면 새로 설정
	MeshComp->SetMaterial(0, FloorMaterial);

	//초기값 설정
	FloorMID = MeshComp->CreateDynamicMaterialInstance(0);
	if (IsValid(FloorMID)) {
		FloorMID->SetScalarParameterValue(TEXT("InterpRatio"), 0);
	}
	CurrentAlpha = 0;
	DeltaAlpha = 1 / (ChangeDuration * 10);
	
}

//페이즈 진입시 이펙트 설정
void ABossFloorEffect::StartEffect(ALuxeaterEnemy* Enemy) {
	if (!GetWorld()->GetTimerManager().IsTimerActive(UpdateEndTimerHandle)) {
		GetWorld()->GetTimerManager().SetTimer(UpdateEndTimerHandle, this, &ABossFloorEffect::FinishUpdate, ChangeDuration, false);
	}
	if (!GetWorld()->GetTimerManager().IsTimerActive(MaterialUpdateTimerHandle)) {
		GetWorld()->GetTimerManager().SetTimer(MaterialUpdateTimerHandle, this, &ABossFloorEffect::UpdateMaterial, 0.1, true);
	}
}

//보스 사망시 이펙트 해제
void ABossFloorEffect::ClearEffect(AActor* EndPlayActor, const EEndPlayReason::Type EndPlayReason) {
	if (!GetWorld()->GetTimerManager().IsTimerActive(UpdateEndTimerHandle)) {
		GetWorld()->GetTimerManager().SetTimer(UpdateEndTimerHandle, this, &ABossFloorEffect::FinishUpdate, ChangeDuration, false);
		CurrentAlpha *= -1;
	}
	if (!GetWorld()->GetTimerManager().IsTimerActive(MaterialUpdateTimerHandle)) {
		GetWorld()->GetTimerManager().SetTimer(MaterialUpdateTimerHandle, this, &ABossFloorEffect::UpdateMaterial, 0.1, true);
	}
}

//천천히 이펙트 머티리얼로 전환
void ABossFloorEffect::UpdateMaterial() {
	CurrentAlpha += DeltaAlpha;
	if (IsValid(FloorMID)) {
		FloorMID->SetScalarParameterValue(TEXT("InterpRatio"), CurrentAlpha);
	}
}

//업데이트 완료시 타이머 해제
void ABossFloorEffect::FinishUpdate() {
	if (GetWorld()->GetTimerManager().IsTimerActive(MaterialUpdateTimerHandle)) {
		GetWorld()->GetTimerManager().ClearTimer(MaterialUpdateTimerHandle);
	}
}