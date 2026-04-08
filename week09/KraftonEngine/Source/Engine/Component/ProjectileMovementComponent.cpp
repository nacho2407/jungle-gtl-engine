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

	if (Velocity.Length() > FMath::Epsilon || InitialSpeed <= 0.0f)
	{
		return;
	}

	USceneComponent* UpdatedSceneComponent = GetUpdatedComponent();
	if (!UpdatedSceneComponent)
	{
		return;
	}

	Velocity = UpdatedSceneComponent->GetForwardVector().Normalized() * InitialSpeed;
}

void UProjectileMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	UMovementComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

	USceneComponent* UpdatedSceneComponent = GetUpdatedComponent();
	if (!UpdatedSceneComponent)
	{
		return;
	}

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
}

void UProjectileMovementComponent::Serialize(FArchive& Ar)
{
	UMovementComponent::Serialize(Ar);
	Ar << Velocity;
	Ar << InitialSpeed;
	Ar << MaxSpeed;
}

void UProjectileMovementComponent::StopSimulating()
{
	Velocity = FVector(0.0f, 0.0f, 0.0f);
}

FVector UProjectileMovementComponent::GetPreviewVelocity() const
{
	FVector PreviewVelocity = Velocity;

	if (PreviewVelocity.Length() <= FMath::Epsilon && InitialSpeed > 0.0f)
	{
		USceneComponent* SourceComponent = GetUpdatedComponent();
		if (!SourceComponent)
		{
			AActor* OwnerActor = GetOwner();
			SourceComponent = OwnerActor ? OwnerActor->GetRootComponent() : nullptr;
		}

		if (SourceComponent)
		{
			PreviewVelocity = SourceComponent->GetForwardVector().Normalized() * InitialSpeed;
		}
	}

	const float CurrentSpeed = PreviewVelocity.Length();
	if (MaxSpeed > 0.0f && CurrentSpeed > MaxSpeed)
	{
		PreviewVelocity = PreviewVelocity.Normalized() * MaxSpeed;
	}

	return PreviewVelocity;
}

EProjectileHitBehavior UProjectileMovementComponent::GetHitBehavior() const
{
	return EProjectileHitBehavior::Stop;
}

bool UProjectileMovementComponent::HandleBlockingHit(USceneComponent* UpdatedSceneComponent, const FVector& CurrentLocation, const FVector& MoveDelta, const FHitResult& HitResult)
{
	(void)UpdatedSceneComponent;
	(void)CurrentLocation;
	(void)MoveDelta;
	(void)HitResult;
	return true;
}
