//WeaponBase.cpp
#include "Weapon/WeaponBase.h"
#include "Components/SphereComponent.h"
#include "Components/ArrowComponent.h"
#include "OblivioCharacter.h"
#include "OblivioComponents/LightAttackComponent.h"
#include "Animation/AnimInstance.h"

// Sets default values
AWeaponBase::AWeaponBase()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	SphereComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
	RootComponent = SphereComp;
	SphereComp->SetSimulatePhysics(false);
	SphereComp->SetCollisionProfileName("NoCollision");

	ArrowComp = CreateDefaultSubobject<UArrowComponent>(TEXT("ArrowComp"));
	ArrowComp->SetupAttachment(RootComponent);

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetupAttachment(RootComponent);
	MeshComp->SetCollisionProfileName("NoCollision");
	MeshComp->SetSimulatePhysics(false);
}

// Called when the game starts or when spawned
void AWeaponBase::BeginPlay()
{
	Super::BeginPlay();
	//UseWeapon();
}

// Called every frame
void AWeaponBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

//무기 사용 시작, 애니메이션을 이용해 타이밍을 조절할경우 플레이어 측에서 AnimNotify를 이용해 ExecuteWeaponAttack() 재호출
bool AWeaponBase::UseWeapon()
{
	UE_LOG(LogTemp, Warning, TEXT("Animate weapon usage"));
	
	bool IsAnimating = PlayWeaponAnim();
	if (IsAnimating) {
		MeshComp->SetVisibility(true);
	}
	return IsAnimating;
}

//실제 공격 호출(빛, 탄환생성등)
void AWeaponBase::ExecuteWeaponAttack(FVector TargetLocation)
{

}

//무기 사용 중단
void AWeaponBase::StopWeapon()
{
	//LightAttackComp->TurnOffLight();
}

//LightComponent 각도 변경
void AWeaponBase::ChangeWeaponAngle(float DeltaAngle)
{
	//UE_LOG(LogTemp, Warning, TEXT("ChangeWeaponAngle Called"));
	//LightAttackComp->ChangeLightAngle(DeltaAngle);
}

//무기 애니메이션 재생, 재생 시작 여부 반환
bool AWeaponBase::PlayWeaponAnim()
{
	if (!IsValid(WeaponAnim)) return false;

	AOblivioCharacter* Onwer = Cast<AOblivioCharacter>(GetOwner());
	if (!IsValid(Onwer)) return false;

	UAnimInstance* Anim = Onwer->GetMesh()->GetAnimInstance();

	if (Anim && !Anim->Montage_IsPlaying(WeaponAnim)) {
		Onwer->PlayAnimMontage(WeaponAnim);
		return true;
	}
	else {
		return false;
	}
}
