//LightAttackComponent.cpp

#include "OblivioComponents/LightAttackComponent.h"
#include "AIEnemy/EnemyBase.h"

#include "Engine/OverlapResult.h"
#include "Components/CapsuleComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

// Sets default values for this component's properties
ULightAttackComponent::ULightAttackComponent()
{
    // Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
    // off to improve performance if you don't need them.
    PrimaryComponentTick.bCanEverTick = false;
    LightTime = 1.f; 
    LightIntensityScale = 5000.f;

    DistancePerAngle = 0.5f;
    DamagePerAngle = 0.5f;
    MinAngle = 30;
    MinAngle = 160;

    Damage = 100;
    bIsConcentrated = false;
    LightAngle = 30;
    LightDistance = 500;
    MaxDamageDistance = 100;
    DamageAttenuationRate = 1.f;
    BasicLightColor = FColor::White;
}
void ULightAttackComponent::OnRegister()
{
    Super::OnRegister();

    // 
    if (!IsValid(SpotLightComp))
    {
        UE_LOG(LogTemp, Warning, TEXT("Registering Spotlight"));
        SpotLightComp = NewObject<USpotLightComponent>(this, TEXT("SpotLightComp"));
        SpotLightComp->SetupAttachment(this);
        SpotLightComp->RegisterComponent();
        SpotLightComp->LightColor = BasicLightColor;
    }
    if (!IsValid(PointLightComp))
    {
        UE_LOG(LogTemp, Warning, TEXT("Registering PointLightComp"));
        PointLightComp = NewObject<UPointLightComponent>(this, TEXT("PointLightComp"));
        PointLightComp->SetupAttachment(this);
        PointLightComp->RegisterComponent();
        PointLightComp->LightColor = BasicLightColor;
    }
}

// Called when the game starts
void ULightAttackComponent::BeginPlay()
{
    Super::BeginPlay();

    //광원 출력 수치와 동기화
    if (IsValid(SpotLightComp)) {
        UE_LOG(LogTemp, Warning, TEXT("SpotLightComp light synced"));
        SpotLightComp->SetAttenuationRadius(LightDistance * 10);
        SpotLightComp->SetIntensity(Damage * LightIntensityScale);
        // 시작시 안보이게 꺼놓기
        SpotLightComp->SetVisibility(false);
        if (bIsConcentrated)    //집중형 범위
        {
            SpotLightComp->SetOuterConeAngle(LightAngle / 2.f);
            SpotLightComp->SetInnerConeAngle(LightAngle / 2.f);
        }
    }
    if (IsValid(PointLightComp)) {
        UE_LOG(LogTemp, Warning, TEXT("PointLightComp light synced"));
        PointLightComp->SetAttenuationRadius(LightDistance * 10);
        PointLightComp->SetIntensity(Damage * LightIntensityScale);
        // 시작시 안보이게 꺼놓기
        PointLightComp->SetVisibility(false);
    }
}


// Called every frame
void ULightAttackComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // ...
}

void ULightAttackComponent::CreateLightAttack(FVector SourceLocation, FVector LightDirection)
{
    FVector LightDir = LightDirection.GetSafeNormal();
    float HalfAngle = LightAngle / 2.f;

    //공격범위 시각화
    if (bIsConcentrated)
    {
        // 부채꼴 외곽선 2개
        FVector LeftBoundDir = LightDir.RotateAngleAxis(HalfAngle, FVector::UpVector);
        FVector RightBoundDir = LightDir.RotateAngleAxis(-HalfAngle, FVector::UpVector);

        DrawDebugLine(GetWorld(), SourceLocation, SourceLocation + LeftBoundDir * LightDistance, FColor::Yellow, false, .02f);
        DrawDebugLine(GetWorld(), SourceLocation, SourceLocation + RightBoundDir * LightDistance, FColor::Yellow, false, .02f);
    }
    else
    {
        // 전방위 구형 범위
        DrawDebugSphere(GetWorld(), SourceLocation, LightDistance, 16, FColor::Yellow, false, .5f);
    }

    //광원 출력
    if (bIsConcentrated) {
        if (IsValid(SpotLightComp)) {
            SpotLightComp->SetVisibility(true);
        }
    }
    else {
        if (IsValid(PointLightComp)) {
            PointLightComp->SetVisibility(true);
        }
    }
    GetWorld()->GetTimerManager().SetTimer(LightOffTimerHandle, this, &ULightAttackComponent::TurnOffLight, LightTime, false);

    //LightDistance안의 모든 적 얻기
    TArray<AActor*> OverlapActors;

    UKismetSystemLibrary::SphereOverlapActors(
        GetWorld(),
        SourceLocation,
        LightDistance,
        TArray<TEnumAsByte<EObjectTypeQuery>>(),
        AEnemyBase::StaticClass(),
        TArray<AActor*>{GetOwner()},
        OverlapActors
    );

    if (OverlapActors.IsEmpty()) return;


    //IsConcentrated면 내적으로 각도 안의 적 액터만 남김
    //약간만 걸치는 상황까지 고려해 접선 접점위치까지 하나라도 범위 안이면 대상으로 판정
    TArray<AActor*> Candidates;
    for (AActor* Target : OverlapActors)
    {
        if (!Target) continue;
        if (!bIsConcentrated)
        {
            Candidates.Add(Target);
            continue;
        }

        FVector Center = Target->GetActorLocation();
        FVector ToTarget = (Center - SourceLocation);
        float Distance = ToTarget.Size();
        FVector ToTargetNorm = ToTarget.GetSafeNormal();

        float CapsuleRadius = 30.f;
        UCapsuleComponent* Capsule = Target->FindComponentByClass<UCapsuleComponent>();
        if (Capsule) CapsuleRadius = Capsule->GetScaledCapsuleRadius();

        //중심각 및 접선 각도 계산, 각도로 범위 판단 가능하니 접점 계산은 생략
        float CenterAngle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(LightDir, ToTargetNorm)));
        float SinTheta = FMath::Clamp(CapsuleRadius / Distance, 0.f, 1.f);
        float TangentAngle = FMath::RadiansToDegrees(FMath::Asin(SinTheta));

        //각도가 범위 안이면 목표목록에 등록
        if (CenterAngle - TangentAngle <= HalfAngle)
        {
            Candidates.Add(Target);
        }
    }

    if (Candidates.IsEmpty()) return;
    UE_LOG(LogTemp, Warning, TEXT("After Filtering, %d actors left in the light angle"), Candidates.Num());
    //각 액터에 대해 중앙과 캡슐 컴포넌트 접점 2개에 대해 linetrace 진행
    //linetrace 충돌한 액터가 대상 적 액터면 데미지 판정 진행, maxdistance까진 Damage그대로, maxDamageDistance이후로는 데미지 감쇠
    const UObject* WorldContext = GetWorld();
    TSet<AActor*> HitActors;

    for (AActor* Target : Candidates)
    {
        if (HitActors.Contains(Target)) continue;

        //적까지의 벡터 계산
        FVector Center = Target->GetActorLocation();
        FVector ToTarget = (Center - SourceLocation);
        float Distance = ToTarget.Size();
        FVector ToTargetNorm = ToTarget.GetSafeNormal();

        //적 캡슐 반경 접점 계산
        float CapsuleRadius = 30.f;
        UCapsuleComponent* Capsule = Target->FindComponentByClass<UCapsuleComponent>();
        if (Capsule) CapsuleRadius = Capsule->GetScaledCapsuleRadius();

        float SinTheta = FMath::Clamp(CapsuleRadius / Distance, 0.f, 1.f);
        float TangentAngle = FMath::RadiansToDegrees(FMath::Asin(SinTheta));

        FVector LeftTangentDir = ToTargetNorm.RotateAngleAxis(TangentAngle, FVector::UpVector);
        FVector RightTangentDir = ToTargetNorm.RotateAngleAxis(-TangentAngle, FVector::UpVector);

        float TangentDist = Distance * FMath::Cos(FMath::DegreesToRadians(TangentAngle));
        FVector LeftTangentPoint = SourceLocation + LeftTangentDir * TangentDist;
        FVector RightTangentPoint = SourceLocation + RightTangentDir * TangentDist;

        //중심과 2개접점, 총 3개 지점에 대한 라인트레이스 진행
        for (const FVector& TraceTarget : { Center, LeftTangentPoint, RightTangentPoint })
        {
            FHitResult HitResult;

            bool bHit = UKismetSystemLibrary::LineTraceSingle(
                WorldContext,
                SourceLocation,
                TraceTarget,
                UEngineTypes::ConvertToTraceType(ECC_Visibility),
                false,
                TArray<AActor*>{GetOwner()},
                EDrawDebugTrace::ForDuration,
                HitResult,
                true,
                FLinearColor::Red,
                FLinearColor::Green,
                .02f);

            //적이면 데미지 계산 진행
            if (!bHit || HitResult.GetActor() == Target)
            {
                HitActors.Add(Target);

                float FinalDamage = 0.f;

                if (Distance <= MaxDamageDistance)
                {
                    FinalDamage = Damage;
                }
                else
                {
                    float AttenuationRatio = 1.f - FMath::Clamp((Distance - MaxDamageDistance) / (LightDistance - MaxDamageDistance), 0.f, 1.f);
                    FinalDamage = Damage * AttenuationRatio * DamageAttenuationRate;
                }
                //데미지 호출
                UGameplayStatics::ApplyDamage(Target, FinalDamage, nullptr, GetOwner(), nullptr);
                UE_LOG(LogTemp, Warning, TEXT("Applying %f damage to the enemy %s!"), FinalDamage, *Target->GetName());
                //빛판정 호출
                if (AEnemyBase* TargetEnemy = Cast<AEnemyBase>(Target)) {
                    UE_LOG(LogTemp, Warning, TEXT("Calling Light event of the enemy %s!"), *Target->GetName());
                    TargetEnemy->OnLightHit(FinalDamage/Damage, 0.1f);
                }
                //하나 성공시 추가 라인트레이스 불필요.
                break;
            }
        }
    }
}

void ULightAttackComponent::TurnOffLight()
{
    if (GetWorld()->GetTimerManager().IsTimerActive(LightOffTimerHandle)) {
        GetWorld()->GetTimerManager().ClearTimer(LightOffTimerHandle);
    }
    if (IsValid(SpotLightComp)) SpotLightComp->SetVisibility(false);
    if (IsValid(PointLightComp)) PointLightComp->SetVisibility(false);
}

void ULightAttackComponent::ChangeLightAngle(float Angle)
{
    UE_LOG(LogTemp, Warning, TEXT("Updating LightAngle"));
    if (IsValid(SpotLightComp) && bIsConcentrated) {
        if (LightAngle + Angle > MinAngle && LightAngle + Angle < MaxAngle) {
            //빛 각도 조절
            LightAngle = LightAngle + Angle;
            SpotLightComp->SetOuterConeAngle(LightAngle / 2);
            SpotLightComp->SetInnerConeAngle(LightAngle / 2);

            //빛 세기 조절
            LightDistance -= Angle * DistancePerAngle;
            Damage -= Angle * DamagePerAngle;
            SpotLightComp->SetAttenuationRadius(LightDistance);
            SpotLightComp->SetIntensity(Damage * LightIntensityScale);
        }
    }
}

