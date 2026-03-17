#include "Component/USceneComponent.h"
#include "Math/FMatrix.h"

USceneComponent::USceneComponent()
    : UActorComponent()
    , RelativeLocation(FVector::ZeroVector)
    , RelativeRotationEuler(FVector::ZeroVector)
    , RelativeRotationQuat(FQuat::Identity())
    , RelativeScale(FVector::OneVector)
    , bWorldTransformDirty(true)
    , CachedWorldTransform(FMatrix::Identity)
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
    MarkTransformDirty();
}

const FVector& USceneComponent::GetRelativeLocation() const
{
    return RelativeLocation;
}

void USceneComponent::SetRelativeRotation(const FVector& InRotationEuler)
{
    RelativeRotationEuler = InRotationEuler;
    RelativeRotationQuat = FQuat::FromEulerXYZ(InRotationEuler);
    RelativeRotationQuat.Normalize();
    MarkTransformDirty();
}

const FVector& USceneComponent::GetRelativeRotation() const
{
    return RelativeRotationEuler;
}

void USceneComponent::SetRelativeRotationQuat(const FQuat& InQuat)
{
    RelativeRotationQuat = InQuat;
    RelativeRotationQuat.Normalize();

    // 패널 표시용 캐시
    RelativeRotationEuler = RelativeRotationQuat.ToEulerXYZ();

    MarkTransformDirty();
}

const FQuat& USceneComponent::GetRelativeRotationQuat() const
{
    return RelativeRotationQuat;
}

void USceneComponent::SetRelativeScale(const FVector& InScale)
{
    RelativeScale = InScale;
    MarkTransformDirty();
}

const FVector& USceneComponent::GetRelativeScale() const
{
    return RelativeScale;
}
FMatrix USceneComponent::GetWorldTransformMatrix() const
{
    UpdateWorldTransformIfNeeded();
    return CachedWorldTransform;
}

void USceneComponent::MarkTransformDirty()
{
    bWorldTransformDirty = true;
}

void USceneComponent::UpdateWorldTransformIfNeeded() const
{
    if (!bWorldTransformDirty)
    {
        return;
    }

    FMatrix Scale = FMatrix::MakeScale(RelativeScale);
	FMatrix Rotation = RelativeRotationQuat.ToMatrix();
    FMatrix Translation = FMatrix::MakeTranslation(RelativeLocation);

    CachedWorldTransform = Scale * Rotation * Translation;

    bWorldTransformDirty = false;
}