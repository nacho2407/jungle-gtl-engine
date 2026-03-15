#include "Component/USceneComponent.h"
#include "Math/FMatrix.h"

USceneComponent::USceneComponent()
    : RelativeLocation(FVector::ZeroVector)
    , RelativeRotation(FVector::ZeroVector)
    , RelativeScale(FVector::OneVector)
{
}

USceneComponent::~USceneComponent()
{
}

void USceneComponent::BeginPlay()
{
    UActorComponent::BeginPlay();
}

void USceneComponent::TickComponent(float DeltaTime)
{
    UActorComponent::TickComponent(DeltaTime);
}

void USceneComponent::SetRelativeLocation(const FVector& InLocation)
{
    RelativeLocation = InLocation;
}

const FVector& USceneComponent::GetRelativeLocation() const
{
    return RelativeLocation;
}

void USceneComponent::SetRelativeRotation(const FVector& InRotation)
{
    RelativeRotation = InRotation;
}

const FVector& USceneComponent::GetRelativeRotation() const
{
    return RelativeRotation;
}

void USceneComponent::SetRelativeScale(const FVector& InScale)
{
    RelativeScale = InScale;
}

const FVector& USceneComponent::GetRelativeScale() const
{
    return RelativeScale;
}
FMatrix USceneComponent::GetWorldTransformMatrix() const
{
    FMatrix Scale = FMatrix::MakeScale(RelativeScale);
    FMatrix Rotation = FMatrix::MakeRotationXYZ(RelativeRotation);
	FMatrix Translation = FMatrix::MakeTranslation(RelativeLocation);

	//FMatrix RotationX = FMatrix::MakeRotationX(RelativeRotation.X);
	//FMatrix RotationY = FMatrix::MakeRotationY(RelativeRotation.Y);
	//FMatrix RotationZ = FMatrix::MakeRotationZ(RelativeRotation.Z);
	//FMatrix Rotation = RotationZ * RotationY * RotationX;

	//FMatrix Scale = FMatrix::MakeScale(RelativeScale);

	return Scale * Rotation * Translation;
}
