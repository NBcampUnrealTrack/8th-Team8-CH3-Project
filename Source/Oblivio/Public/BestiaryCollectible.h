// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BestiaryCollectible.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class UNiagaraComponent;
class AOblivioCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBestiaryCollectedSignature, AOblivioCharacter*, Player);

UCLASS()
class OBLIVIO_API ABestiaryCollectible : public AActor
{
	GENERATED_BODY()
	
public:
	ABestiaryCollectible();

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* DocumentMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USphereComponent* InteractSphere;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UNiagaraComponent* HighlightEffect;

    // 이 액터를 주웠을 때 해금될 몬스터의 ID
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bestiary")
    FName TargetMonsterID;

public:
    void BeginPlay() override;
    UPROPERTY(BlueprintAssignable, Category = "Bestiary|Events")
    FOnBestiaryCollectedSignature OnBestiaryCollected;

    // 플레이어가 상호작용했을 때 호출될 함수
    UFUNCTION(BlueprintCallable, Category = "Bestiary")
    void CollectDocument(class AOblivioCharacter* Player);

private:
    UFUNCTION()
    void OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);
};
