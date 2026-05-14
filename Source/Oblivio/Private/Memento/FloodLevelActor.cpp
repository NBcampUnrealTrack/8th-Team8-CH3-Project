#include "Memento/FloodLevelActor.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "OblivioCharacter.h"
#include "Blueprint/UserWidget.h"
#include "Components/AudioComponent.h"

AFloodLevelActor::AFloodLevelActor()
{
	PrimaryActorTick.bCanEverTick = true;

	// 수면 메쉬 생성 및 루트 설정
	WaterMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WaterMesh"));
	RootComponent = WaterMesh;
	WaterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// 물은 처음엔 바닥 아래 있거나 아주 낮은 곳에 위치
	CurrentFloodHeight = 0.0f;
}

void AFloodLevelActor::BeginPlay()
{
	Super::BeginPlay();

	// 플레이어 캐릭터 미리 캐싱 (성능 최적화)
	PlayerRef = Cast<AOblivioCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));

	// 현재 위치를 초기 수위로 설정
	CurrentFloodHeight = GetActorLocation().Z;
}

void AFloodLevelActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsFlooding)
	{
		CurrentFloodHeight = FMath::FInterpConstantTo(CurrentFloodHeight, TargetFloodHeight, DeltaTime, FloodRiseSpeed);

		FVector NewLocation = GetActorLocation();
		NewLocation.Z = CurrentFloodHeight;
		SetActorLocation(NewLocation);

		if (PlayerRef)
		{
			PlayerRef->CurrentWaterLevel = CurrentFloodHeight;
		}

		if (FMath::IsNearlyEqual(CurrentFloodHeight, TargetFloodHeight, 0.1f))
		{
			bIsFlooding = false;
		}
	}
}

void AFloodLevelActor::StartFloodEvent()
{
	if (bIsFlooding) return;

	bIsFlooding = true;

	if (TimerWidgetClass)
	{
		if (TimerWidgetClass)
		{
			ActiveTimerWidget = CreateWidget<UUserWidget>(GetWorld(), TimerWidgetClass);
			if (ActiveTimerWidget) ActiveTimerWidget->AddToViewport();
		}
	}

	if (FloodStartSound)
	{
		UGameplayStatics::PlaySound2D(GetWorld(), FloodStartSound);
	}

	//여자 울음소리 루프 시작
	if (WomanCryingLoop)
	{
		CryingAudioComp = UGameplayStatics::SpawnSound2D(GetWorld(), WomanCryingLoop);
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("경고: 수위가 차오르기 시작합니다! 1분 안에 탈출하십시오!"));
	}
}
void AFloodLevelActor::StopFloodEffects()
{
	bIsFlooding = false;

	if (CryingAudioComp && CryingAudioComp->IsPlaying())
	{
		CryingAudioComp->FadeOut(2.0f, 0.0f);
	}

	if (ActiveTimerWidget)
	{
		ActiveTimerWidget->RemoveFromParent();
		ActiveTimerWidget = nullptr;
	}
}