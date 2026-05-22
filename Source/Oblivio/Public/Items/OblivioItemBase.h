#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Sound/SoundBase.h"
#include "OblivioItemBase.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMementoCollected, class AOblivioCharacter*, Interactor);

UENUM(BlueprintType)
enum class EItemCategory : uint8
{
	Consumable UMETA(DisplayName = "Consumable"),
	Resource   UMETA(DisplayName = "Resource"),
	Equipment  UMETA(DisplayName = "Equipment"),
	Special    UMETA(DisplayName = "Special")
};

UENUM(BlueprintType)
enum class EItemType : uint8
{
	Wood    UMETA(DisplayName = "Wood"),
	Iron    UMETA(DisplayName = "Iron"),
	Food    UMETA(DisplayName = "Food"),
	Water   UMETA(DisplayName = "Water"),
	Battery UMETA(DisplayName = "Battery"),
    Key     UMETA(DisplayName = "Key"),
    Memento UMETA(DisplayName = "Memento"),
    Bottle  UMETA(DisplayName = "Bottle")
};
class UNiagaraComponent;
UCLASS()
class OBLIVIO_API AOblivioItemBase : public AActor
{
	GENERATED_BODY()
	
public:
    AOblivioItemBase();

protected:
    virtual void BeginPlay() override;

    /** 오버랩 발생 시 호출될 함수 */
    UFUNCTION()
    virtual void OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);
    UFUNCTION()
    virtual void OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

public:
    // ==========================================
    // Components
    // ==========================================
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class UStaticMeshComponent* ItemMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class USphereComponent* InteractionSphere;

    // ==========================================
    // Item Data
    // ==========================================
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Settings")
    FName ItemID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Settings")
    EItemCategory ItemCategory;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Settings")
    FText ItemName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Settings", meta = (MultiLine = true))
    FText ItemDescription;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Settings")
    int32 MaxQuantity = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Settings")
    int32 CurrentQuantity = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Settings")
    class UTexture2D* ItemIcon;

    /** 버릴 때 스폰할 액터 클래스 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Settings")
    TSubclassOf<AOblivioItemBase> ItemBPClass;

    /** 기존 타입 구분용 (로직 호환성) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Settings")
    EItemType ItemType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Settings")
    float RestoreValue = 30.0f;

    // 이 아이템을 주웠을 때 연쇄적으로 작동시킬 액터들의 배열
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story Events")
    TArray<AActor*> LinkedActors;

	/**
	 * Memento 픽업 흐름(ActorHasTag "Memento")에서만 처리된다.
	 * 당직표 읽는 순간처럼 충격음·관찰실 시선 고정·패드 진동.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Memento Read Moment")
	bool bApplyDutyReadMomentOnPickup = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Memento Read Moment")
	TObjectPtr<USoundBase> DutyReadBangSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Memento Read Moment")
	bool bDutyReadPlayDynamicForceFeedback = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Memento Read Moment", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DutyReadForceFeedbackIntensity = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Memento Read Moment", meta = (ClampMin = "0.0"))
	float DutyReadForceFeedbackDuration = 0.22f;

	/** 비어 있으면 아래 태그로 월드에서 첫 번째 액터를 찾는다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Memento Read Moment|Observation")
	TObjectPtr<AActor> DutyReadObservationLookActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Memento Read Moment|Observation")
	FName DutyReadObservationActorTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Memento Read Moment|Observation", meta = (ClampMin = "0.05"))
	float DutyReadForcedLookDuration = 2.5f;

	/** 켜 두면 마우스 커서 추적 대신 Observation 쪽 시선 고정 시간 동안 회전 우선 적용된다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Memento Read Moment|Observation")
	bool bDutyReadForceLookTowardsObservation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Memento Read Moment|Observation")
	float DutyReadForcedLookInterpSpeed = 14.0f;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnMementoCollected OnMementoCollected;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
    void OnInteract(AActor* Interactor);
    virtual void OnInteract_Implementation(AActor* Interactor);

protected:
    // [추가] 나이아가라 루팅 이펙트 컴포넌트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Effect")
    UNiagaraComponent* LootNiagaraComponent;
};
