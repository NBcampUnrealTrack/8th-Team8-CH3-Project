#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OblivioItemBase.generated.h"

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
	Battery UMETA(DisplayName = "Battery")
};

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
};
