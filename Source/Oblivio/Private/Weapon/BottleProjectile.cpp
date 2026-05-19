// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon/BottleProjectile.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"

ABottleProjectile::ABottleProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ABottleProjectile::BeginPlay()
{
	Super::BeginPlay();
	//초기 회전값 설정
	SpinVelocity += FVector(0,
		FMath::RandRange(-ImpactRotationRange, ImpactRotationRange),
		FMath::RandRange(-ImpactRotationRange, ImpactRotationRange));
}

//충돌 오버라이드, 기본기능에 추가적으로 깨지는 사운드/이펙트 출력후 파괴
void ABottleProjectile::PlayReflectionSound(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	UE_LOG(LogTemp, Warning, TEXT("hit event!"));
	Super::PlayReflectionSound(HitComp, OtherActor, OtherComp, NormalImpulse, Hit);
	if (IsValid(ExplosionSound)) {
		UGameplayStatics::PlaySoundAtLocation(
			GetWorld(),
			ExplosionSound,
			GetActorLocation());
	}
	if (IsValid(ExplosionParticle)) {
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(),
			ExplosionParticle,
			GetActorLocation());
	}
	Destroy();
}
