#include "Crafting/CraftingReflector.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "OblivioCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/World.h"

ACraftingReflector::ACraftingReflector()
{
	PrimaryActorTick.bCanEverTick = true;

	MaxHealth = 120.0f;
	CurrentHealth = MaxHealth;

	WoodCost = 0;
	IronCost = 3;

	ReflectionIntensity = 5000.0f;
	ReflectionConeAngle = 30.0f;

	ReflectedLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("ReflectedLight"));
	ReflectedLight->SetupAttachment(RootComponent);

	ReflectedLight->SetIntensity(0.0f);
	ReflectedLight->InnerConeAngle = ReflectionConeAngle * 0.5f;
	ReflectedLight->OuterConeAngle = ReflectionConeAngle;
}

void ACraftingReflector::BeginPlay()
{
	Super::BeginPlay();
}

void ACraftingReflector::OnPlaced()
{
	Super::OnPlaced();

	if (ReflectedLight)
	{
		ReflectedLight->SetIntensity(0.0f);
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, TEXT("Reflector Placed: Ready to bounce light!"));
	}
}

void ACraftingReflector::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bIsPlaced) return;

	// 플레이어와 손전등 켜짐 여부 확인
	AOblivioCharacter* Player = Cast<AOblivioCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
	if (!Player || !Player->bIsFlashlightOn)
	{
		ReflectedLight->SetIntensity(0.0f); // 손전등 끄면 반사광도 꺼짐
		return;
	}

	//반사판에 맞는지 검사
	FVector TraceStart = Player->GetActorLocation() + FVector(0.f, 0.f, 40.f); // 손전등 대략적 위치
	FVector TraceDir = Player->GetActorForwardVector(); // 바라보는 방향
	FVector TraceEnd = TraceStart + (TraceDir * 2000.0f); // 빛이 닿는 최대 거리

	FHitResult HitResult;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Player); // 플레이어 자신은 무시

	if (GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, Params))
	{
		if (HitResult.GetActor() == this)
		{
			FVector ReflectionDir = FMath::GetReflectionVector(TraceDir, HitResult.ImpactNormal);

			ReflectedLight->SetWorldLocation(HitResult.ImpactPoint + (HitResult.ImpactNormal * 10.0f));
			ReflectedLight->SetWorldRotation(ReflectionDir.Rotation());
			ReflectedLight->SetIntensity(ReflectionIntensity);
			return;
		}
	}

	// 빛이 엇나갔거나 닿지 않으면 반사광 끄기
	ReflectedLight->SetIntensity(0.0f);
}
