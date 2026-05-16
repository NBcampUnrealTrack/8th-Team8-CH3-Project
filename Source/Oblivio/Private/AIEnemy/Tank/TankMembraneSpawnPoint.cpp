#include "AIEnemy/Tank/TankMembraneSpawnPoint.h"
#include "AIEnemy/Tank/TankMembraneSpawnSubsystem.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"

#if WITH_EDITORONLY_DATA
#include "Components/ArrowComponent.h"
#include "Components/BillboardComponent.h"
#include "UObject/ConstructorHelpers.h"
#endif

ATankMembraneSpawnPoint::ATankMembraneSpawnPoint()
{
	PrimaryActorTick.bCanEverTick = false;

	RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	SetRootComponent(RootScene);

#if WITH_EDITORONLY_DATA
	EditorBillboard = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("EditorBillboard"));
	if (EditorBillboard)
	{
		EditorBillboard->SetupAttachment(RootScene);
		// S_TargetPoint 는 UE 표준 에디터 리소스로 항상 존재한다. 못 찾으면 기본 빌보드 스프라이트가 사용됨.
		static ConstructorHelpers::FObjectFinder<UTexture2D> Icon(
			TEXT("/Engine/EditorResources/S_TargetPoint.S_TargetPoint"));
		if (Icon.Succeeded())
		{
			EditorBillboard->Sprite = Icon.Object;
		}
		EditorBillboard->bIsScreenSizeScaled = true;
	}

	EditorArrow = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("EditorArrow"));
	if (EditorArrow)
	{
		EditorArrow->SetupAttachment(RootScene);
		EditorArrow->ArrowColor = FColor(180, 80, 200);
		EditorArrow->ArrowSize = 1.5f;
		EditorArrow->bTreatAsASprite = false;
	}
#endif
}

void ATankMembraneSpawnPoint::BeginPlay()
{
	Super::BeginPlay();

	UWorld* const World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[TankMembraneSpawnPoint] %s BeginPlay 에서 World 가 null — 등록 실패"),
			*GetNameSafe(this));
		return;
	}

	UTankMembraneSpawnSubsystem* const Sub = World->GetSubsystem<UTankMembraneSpawnSubsystem>();
	if (!Sub)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[TankMembraneSpawnPoint] %s 등록 실패 — UTankMembraneSpawnSubsystem 인스턴스가 없음"),
			*GetNameSafe(this));
		return;
	}

	Sub->RegisterPoint(this);
	UE_LOG(LogTemp, Verbose,
		TEXT("[TankMembraneSpawnPoint] %s 등록 완료 — 현재 등록 개수=%d"),
		*GetNameSafe(this), Sub->GetRegisteredPointCount());
}

void ATankMembraneSpawnPoint::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* const World = GetWorld())
	{
		if (UTankMembraneSpawnSubsystem* const Sub = World->GetSubsystem<UTankMembraneSpawnSubsystem>())
		{
			Sub->UnregisterPoint(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

FTransform ATankMembraneSpawnPoint::GetSpawnTransform() const
{
	return GetActorTransform();
}
