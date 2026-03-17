#include "Component/USceneComponent.h"
#include "Math/FMatrix.h"

USceneComponent::USceneComponent()
    : RelativeLocation(FVector::ZeroVector)
    , RelativeRotation(FVector::ZeroVector)
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

void USceneComponent::SetRelativeRotation(const FVector& InRotation)
{
    RelativeRotation = InRotation;
    MarkTransformDirty();
}

const FVector& USceneComponent::GetRelativeRotation() const
{
    return RelativeRotation;
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

void USceneComponent::MarkTransformDirty() // (*) 근데 왜 이름이 dirty일까?
{
    bWorldTransformDirty = true;

    for (USceneComponent* Child : Children)
    {
        if (Child)
        {
            Child->MarkTransformDirty();
        }
    }
}

void USceneComponent::UpdateWorldTransformIfNeeded() const
{
    if (!bWorldTransformDirty)
    {
        return;
    }

    FMatrix Scale = FMatrix::MakeScale(RelativeScale);
    FMatrix Rotation = FMatrix::MakeRotationXYZ(RelativeRotation);
    FMatrix Translation = FMatrix::MakeTranslation(RelativeLocation);

    CachedWorldTransform = Scale * Rotation * Translation;

     if (ParentComponent)
     {
         CachedWorldTransform = CachedWorldTransform * ParentComponent->GetWorldTransformMatrix();
     }

    bWorldTransformDirty = false;
}

void USceneComponent::SetupAttachment(USceneComponent* InParent)
{
    ParentComponent = InParent;
    if (ParentComponent)
    {
        ParentComponent->Children.push_back(this);
    }
}

USceneComponent* USceneComponent::GetParentComponent() const
{
    return ParentComponent;
}