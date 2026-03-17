#include "Component/USceneComponent.h"
#include "Math/FMatrix.h"

USceneComponent::USceneComponent()
    : UActorComponent()
    , RelativeLocation(FVector::ZeroVector)
    , RelativeRotation(FVector::ZeroVector)
    , RelativeScale(FVector::OneVector)
    , bWorldTransformDirty(true)
    , CachedWorldTransform(FMatrix::Identity)
{
}

USceneComponent::USceneComponent(const FUObjectInitializer& ObjectInitializer)
    : UActorComponent(ObjectInitializer)
    , RelativeLocation(FVector::ZeroVector)
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

void USceneComponent::MarkTransformDirty()
{
    bWorldTransformDirty = true;

    // TODO: �ڽ� ������Ʈ�� �ִٸ�, �ڽĵ鵵 Dirty ó��
    // for (USceneComponent* Child : Children)
    // {
    //     Child->MarkTransformDirty();
    // }
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

    // TODO: �θ� ������Ʈ�� �ִٸ�, �θ� Ʈ�������� �ռ�
    // if (ParentComponent)
    // {
    //     CachedWorldTransform = CachedWorldTransform * ParentComponent->GetWorldTransformMatrix();
    // }

    bWorldTransformDirty = false;
}