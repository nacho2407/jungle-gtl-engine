#include "ProjectileMovementComponent.h"

#include "Component/SceneComponent.h"
#include "GameFramework/AActor.h"
#include "Math/MathUtils.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

IMPLEMENT_CLASS(UProjectileMovementComponent, UMovementComponent)

void UProjectileMovementComponent::BeginPlay()
{
	UMovementComponent::BeginPlay();

	if (Velocity.Length() <= FMath::Epsilon)
    {
        USceneComponent* SourceComponent = GetUpdatedComponent();
        if (!SourceComponent)
        {
            AActor* OwnerActor = GetOwner();
            SourceComponent = OwnerActor ? OwnerActor->GetRootComponent() : nullptr;
        }

        if (SourceComponent)
        {
            FVector Dir = SourceComponent->GetForwardVector().Normalized();
            Velocity = Dir * InitialSpeed;
        }
    }

	Velocity = Velocity.Normalized() * InitialSpeed; 
}

void UProjectileMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
    UMovementComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

    USceneComponent* UpdatedSceneComponent = GetUpdatedComponent();
    if (!UpdatedSceneComponent)
    {
        return;
    }

    const FVector GravityAccel = FVector(0.f, 0.f, -1.f) * GravityScale * 9.8f;

    Velocity += GravityAccel * DeltaTime;

    const float CurrentSpeed = Velocity.Length();
    if (MaxSpeed > 0.0f && CurrentSpeed > MaxSpeed)
    {
        Velocity = Velocity.Normalized() * MaxSpeed;
    }

    const FVector MoveDelta = Velocity * DeltaTime;
    if (MoveDelta.Length() <= FMath::Epsilon)
    {
        return;
    }

    UpdatedSceneComponent->SetWorldLocation(UpdatedSceneComponent->GetWorldLocation() + MoveDelta);
}

void UProjectileMovementComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UMovementComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Velocity", EPropertyType::Vec3, &Velocity, 0.0f, 0.0f, 1.0f });
	OutProps.push_back({ "Initial Speed", EPropertyType::Float, &InitialSpeed, 0.0f, 0.0f, 10.0f });
	OutProps.push_back({ "Max Speed", EPropertyType::Float, &MaxSpeed, 0.0f, 0.0f, 10.0f });
	OutProps.push_back({ "Gravity Scale", EPropertyType::Float, &GravityScale, 0.0f, 1.0f, 0.01f });
}

void UProjectileMovementComponent::Serialize(FArchive& Ar)
{
	UMovementComponent::Serialize(Ar);
	Ar << Velocity;
	Ar << InitialSpeed;
	Ar << MaxSpeed;
	Ar << GravityScale;
}

void UProjectileMovementComponent::StopSimulating()
{
	Velocity = FVector(0.0f, 0.0f, 0.0f);
}

FVector UProjectileMovementComponent::GetPreviewVelocity() const
{
	return ComputeEffectiveVelocity();
}

EProjectileHitBehavior UProjectileMovementComponent::GetHitBehavior() const
{
	return EProjectileHitBehavior::Stop;
}

FVector UProjectileMovementComponent::ComputeEffectiveVelocity() const
{
	if (Velocity.Length() > FMath::Epsilon)
    {
        return Velocity;
    }

    USceneComponent* SourceComponent = GetUpdatedComponent();
    if (!SourceComponent)
    {
        AActor* OwnerActor = GetOwner();
        SourceComponent = OwnerActor ? OwnerActor->GetRootComponent() : nullptr;
    }

    if (SourceComponent)
    {
        return SourceComponent->GetForwardVector().Normalized() * InitialSpeed;
    }

    return FVector();
}

bool UProjectileMovementComponent::HandleBlockingHit(USceneComponent* UpdatedSceneComponent, const FVector& CurrentLocation, const FVector& MoveDelta, const FHitResult& HitResult)
{
	(void)UpdatedSceneComponent;
	(void)CurrentLocation;
	(void)MoveDelta;
	(void)HitResult;
	return true;
}
