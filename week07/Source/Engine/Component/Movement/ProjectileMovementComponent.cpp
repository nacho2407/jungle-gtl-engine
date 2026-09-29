#include "ProjectileMovementComponent.h"
#include "Component/SceneComponent.h"
#include "Object/ObjectFactory.h"

DEFINE_CLASS(UProjectileMovementComponent, UMovementComponent)
REGISTER_FACTORY(UProjectileMovementComponent)

void UProjectileMovementComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UMovementComponent::GetEditableProperties(OutProps);
    // "Velocity" 는 UMovementComponent::GetEditableProperties 에서 이미 추가됩니다.
    OutProps.push_back({ "Initial Speed",             EPropertyType::Float, &InitialSpeed, 0.0f, 0.0f, 1.0f });
    OutProps.push_back({ "Max Speed",                 EPropertyType::Float, &MaxSpeed,     0.0f, 0.0f, 1.0f });
    OutProps.push_back({ "Gravity Scale",             EPropertyType::Float, &GravityScale, 0.0f, 5.0f, 0.01f });
    OutProps.push_back({ "Rotation Follows Velocity", EPropertyType::Bool,  &bRotationFollowsVelocity });
}

void UProjectileMovementComponent::BeginPlay()
{
	UActorComponent::BeginPlay();

	// 생성 및 발사 시점의 초기 속도 세팅
	if (UpdatedComponent)
	{
		// 별도의 타겟 방향이 없다면 UpdatedComponend의 Forward 방향으로 발사된다.
		if (Velocity.IsNearlyZero())
		{
			Velocity = UpdatedComponent->GetForwardVector() * InitialSpeed;
		}
		else 
		{
			Velocity = Velocity.GetSafeNormal() * InitialSpeed;
		}
		
	}
}

void UProjectileMovementComponent::TickComponent(float DeltaTime)
{
    if (UpdatedComponent == nullptr)
    {
        return;
    }

    // 가속도 연산 (언리얼 단위 스케일 적용)
    FVector Acceleration(0.0f, 0.0f, -9.8f * GravityScale);

	// 가속도에 따라 속도 값을 t에 대해 적분한 뒤 최대 속도를 넘지 않도록 제한한다.
    Velocity += Acceleration * DeltaTime;
	if (IsExceedingMaxSpeed(MaxSpeed))
    {
        Velocity = Velocity.GetSafeNormal() * MaxSpeed;
    }

    // 위치를 적분한 뒤 컴포넌트를 이동시킨다.
    FVector MoveDelta = Velocity * DeltaTime;
    UpdatedComponent->AddWorldOffset(MoveDelta);

	// bRotationFollowsVelocity가 켜져 있으면 투사체가 방향벡터를 바라보도록 한다.
    if (bRotationFollowsVelocity && !Velocity.IsNearlyZero())
    {
        FVector Direction = Velocity.GetSafeNormal();
        
        float YawDegrees = MathUtil::RadiansToDegrees(std::atan2(Direction.Y, Direction.X));
        float FlatLength = std::sqrt(Direction.X * Direction.X + Direction.Y * Direction.Y);
        float PitchDegrees = MathUtil::RadiansToDegrees(std::atan2(Direction.Z, FlatLength));

        // 내부 규약에 맞게 RelativeRotation 값을 갱신한다.
        UpdatedComponent->SetRelativeRotation(FVector(0.0f, PitchDegrees, YawDegrees));
    }
}