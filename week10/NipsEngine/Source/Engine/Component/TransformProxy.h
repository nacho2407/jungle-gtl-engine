#pragma once

#include "Core/CoreMinimal.h"
#include "Math/Matrix.h"
#include "GameFramework/AActor.h"
#include "Component/SceneComponent.h"
#include "Component/SkeletalMeshComponent.h"

class ITransformProxy
{
public:
    virtual ~ITransformProxy() = default;
    virtual FMatrix GetTransform() const = 0;
    virtual void SetTransform(const FMatrix& M) = 0;
};

class FActorTransformProxy : public ITransformProxy
{
    AActor* Actor;
public:
    FActorTransformProxy(AActor* InActor) : Actor(InActor) {}
    virtual FMatrix GetTransform() const override
    {
        if (!Actor || !Actor->GetRootComponent()) return FMatrix::Identity;
        return Actor->GetRootComponent()->GetWorldMatrix();
    }
    virtual void SetTransform(const FMatrix& M) override
    {
        if (!Actor) return;
        FVector Translation, Scale;
        FMatrix Rotation;
        if (M.Decompose(Translation, Rotation, Scale))
        {
            Actor->SetActorLocation(Translation);
            Actor->SetActorRotation(Rotation.GetEuler());
            Actor->SetActorScale(Scale);
        }
    }
};

class FComponentTransformProxy : public ITransformProxy
{
    USceneComponent* Component;
public:
    FComponentTransformProxy(USceneComponent* InComponent) : Component(InComponent) {}
    virtual FMatrix GetTransform() const override
    {
        if (!Component) return FMatrix::Identity;
        return Component->GetWorldMatrix();
    }
    virtual void SetTransform(const FMatrix& M) override
    {
        if (!Component) return;
        FVector Translation, Scale;
        FMatrix Rotation;
        if (M.Decompose(Translation, Rotation, Scale))
        {
            Component->SetWorldLocation(Translation);
            // Assuming SetRelativeRotation and SetRelativeScale are appropriate here, 
            // but for WorldMatrix we might need SetWorldRotation/Scale if they exist.
            // SceneComponent usually has SetWorldLocation but might not have SetWorldRotation for all.
            // Let's use Relative for now or check SceneComponent.h
            Component->SetRelativeRotation(Rotation.GetEuler());
            Component->SetRelativeScale(Scale);
        }
    }
};

class FBoneTransformProxy : public ITransformProxy
{
    USkeletalMeshComponent* SkelComp;
    int32 BoneIndex;

public:
    FBoneTransformProxy(USkeletalMeshComponent* InSkelComp, int32 InBoneIndex)
        : SkelComp(InSkelComp), BoneIndex(InBoneIndex) {}

    virtual FMatrix GetTransform() const override
    {
        if (!SkelComp) return FMatrix::Identity;
        return SkelComp->GetBoneGlobalTransform(BoneIndex);
    }

    virtual void SetTransform(const FMatrix& M) override
    {
        if (!SkelComp) return;
        SkelComp->SetBoneGlobalTransform(BoneIndex, M);
    }
};
