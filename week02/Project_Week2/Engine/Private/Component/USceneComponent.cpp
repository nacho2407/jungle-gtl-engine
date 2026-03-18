#include "Component/USceneComponent.h"
#include "Math/FMatrix.h"

#include <algorithm>
#include <cmath>

USceneComponent::USceneComponent()
    : UActorComponent()
    , RelativeLocation(FVector::ZeroVector)
    , RelativeRotationEuler(FVector::ZeroVector)
    , RelativeRotationQuat(FQuat::Identity())
    , RelativeScale(FVector::OneVector)
    , RelativeTransformMatrix(FMatrix::Identity)
    , bWorldTransformDirty(true)
    , CachedWorldTransform(FMatrix::Identity)
{
    RebuildRelativeMatrixFromTRS();
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

void USceneComponent::RebuildRelativeMatrixFromTRS()
{
    const FMatrix Scale = FMatrix::MakeScale(RelativeScale);
    const FMatrix Rotation = RelativeRotationQuat.ToMatrix();
    const FMatrix Translation = FMatrix::MakeTranslation(RelativeLocation);

    RelativeTransformMatrix = Scale * Rotation * Translation;
}

void USceneComponent::SyncTRSCachesFromMatrixApprox()
{
    RelativeLocation = RelativeTransformMatrix.GetTranslation();

    FVector XAxis(
        RelativeTransformMatrix.M[0][0],
        RelativeTransformMatrix.M[0][1],
        RelativeTransformMatrix.M[0][2]);

    FVector YAxis(
        RelativeTransformMatrix.M[1][0],
        RelativeTransformMatrix.M[1][1],
        RelativeTransformMatrix.M[1][2]);

    FVector ZAxis(
        RelativeTransformMatrix.M[2][0],
        RelativeTransformMatrix.M[2][1],
        RelativeTransformMatrix.M[2][2]);

    const float ScaleX = XAxis.Length();
    const float ScaleY = YAxis.Length();
    const float ScaleZ = ZAxis.Length();

    RelativeScale = FVector(ScaleX, ScaleY, ScaleZ);

    if (ScaleX <= 0.000001f || ScaleY <= 0.000001f || ScaleZ <= 0.000001f)
    {
        return;
    }

    XAxis /= ScaleX;
    YAxis /= ScaleY;
    ZAxis /= ScaleZ;

    XAxis.Normalize();

    YAxis = YAxis - XAxis * YAxis.Dot(XAxis);
    if (YAxis.LengthSquared() > 0.000001f)
    {
        YAxis.Normalize();
    }

    ZAxis = ZAxis - XAxis * ZAxis.Dot(XAxis) - YAxis * ZAxis.Dot(YAxis);
    if (ZAxis.LengthSquared() > 0.000001f)
    {
        ZAxis.Normalize();
    }

    FMatrix Rot = FMatrix::MakeIdentity();
    Rot.M[0][0] = XAxis.X;
    Rot.M[0][1] = XAxis.Y;
    Rot.M[0][2] = XAxis.Z;

    Rot.M[1][0] = YAxis.X;
    Rot.M[1][1] = YAxis.Y;
    Rot.M[1][2] = YAxis.Z;

    Rot.M[2][0] = ZAxis.X;
    Rot.M[2][1] = ZAxis.Y;
    Rot.M[2][2] = ZAxis.Z;

    const float SinY = Rot.M[0][2];
    const float Yaw = std::asin(std::clamp(SinY, -1.0f, 1.0f));

    float Pitch = 0.0f;
    float Roll = 0.0f;

    const float CosY = std::cos(Yaw);
    if (std::fabs(CosY) > 0.0001f)
    {
        Pitch = std::atan2(-Rot.M[1][2], Rot.M[2][2]);
        Roll = std::atan2(-Rot.M[0][1], Rot.M[0][0]);
    }
    else
    {
        Pitch = std::atan2(Rot.M[2][1], Rot.M[1][1]);
        Roll = 0.0f;
    }

    RelativeRotationEuler = FVector(Pitch, Yaw, Roll);
    RelativeRotationQuat = FQuat::FromEulerXYZ(RelativeRotationEuler);
    RelativeRotationQuat.Normalize();
}

void USceneComponent::SetRelativeLocation(const FVector& InLocation)
{
    RelativeLocation = InLocation;
    RebuildRelativeMatrixFromTRS();
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

    RebuildRelativeMatrixFromTRS();
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
    RelativeRotationEuler = RelativeRotationQuat.ToEulerXYZ();

    RebuildRelativeMatrixFromTRS();
    MarkTransformDirty();
}

const FQuat& USceneComponent::GetRelativeRotationQuat() const
{
    return RelativeRotationQuat;
}

void USceneComponent::SetRelativeScale(const FVector& InScale)
{
    RelativeScale = InScale;
    RebuildRelativeMatrixFromTRS();
    MarkTransformDirty();
}

const FVector& USceneComponent::GetRelativeScale() const
{
    return RelativeScale;
}

void USceneComponent::SetRelativeTransformMatrix(const FMatrix& InMatrix)
{
    RelativeTransformMatrix = InMatrix;
    SyncTRSCachesFromMatrixApprox();
    MarkTransformDirty();
}

const FMatrix& USceneComponent::GetRelativeTransformMatrix() const
{
    return RelativeTransformMatrix;
}

void USceneComponent::SetWorldTransformMatrix(const FMatrix& InMatrix)
{
    if (ParentComponent)
    {
        const FMatrix ParentWorld = ParentComponent->GetWorldTransformMatrix();
        const FMatrix ParentWorldInv = FMatrix::InverseAffine(ParentWorld);
        RelativeTransformMatrix = InMatrix * ParentWorldInv;
    }
    else
    {
        RelativeTransformMatrix = InMatrix;
    }

    SyncTRSCachesFromMatrixApprox();
    MarkTransformDirty();
}

FMatrix USceneComponent::GetWorldTransformMatrix() const
{
    UpdateWorldTransformIfNeeded();
    return CachedWorldTransform;
}

void USceneComponent::MarkTransformDirty()
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

    CachedWorldTransform = RelativeTransformMatrix;

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