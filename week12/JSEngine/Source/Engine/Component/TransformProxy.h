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
public:
    virtual FMatrix GetTransform() const override
    {
        if (Actors.empty() || !Actors[0])
            return FMatrix::Identity;

        // 현재는 첫 번째 Actor 를 기준으로 Transform 을 반환하지만 다수 Actor 들의 Center Pivot 등을 지원할 수 있음
        return Actors[0]->GetRootComponent()->GetWorldMatrix();
    }
    virtual void SetTransform(const FMatrix& M) override
    {
        if (Actors.empty())
            return;

        // 기준 (첫 Actor)
        FMatrix OldM = GetTransform();

        FVector OldT, NewT, OldS, NewS;
        FMatrix OldR, NewR;

        OldM.Decompose(OldT, OldR, OldS);
        M.Decompose(NewT, NewR, NewS);

        FVector Delta = NewT - OldT;

        for (AActor* Actor : Actors)
        {
            if (!Actor)
                continue;
            Actor->AddActorWorldOffset(Delta);
        }
    }

	void AddTarget(AActor* InActor)
	{
        Actors.push_back(InActor);
	}

private:
    TArray<AActor*> Actors;
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
            Component->SetRelativeRotationQuat(FQuat(Rotation));
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

class FSocketTransformProxy : public ITransformProxy
{
    USkeletalMeshComponent* SkelComp;
    FName SocketName;

public:
    FSocketTransformProxy(USkeletalMeshComponent* InSkelComp, const FName& InSocketName)
        : SkelComp(InSkelComp), SocketName(InSocketName) {}

    virtual FMatrix GetTransform() const override
    {
        if (!SkelComp)
            return FMatrix::Identity;
        return SkelComp->GetSocketTransform(SocketName).ToMatrixWithScale();
    }

    virtual void SetTransform(const FMatrix& M) override
    {
        if (!SkelComp || !SkelComp->GetSkeletalMesh())
            return;

        FSkeletalMesh* MeshData = SkelComp->GetSkeletalMesh()->GetMeshData();
        if (!MeshData)
            return;

        for (auto& Socket : MeshData->Sockets)
        {
            if (Socket.Name != SocketName)
                continue;

            FTransform TargetWorld(M);
            TargetWorld.NormalizeRotation();

            const FTransform BoneGlobal(SkelComp->GetCurrentGlobalPose()[Socket.BoneIndex]);
            const FTransform ComponentWorld(SkelComp->GetWorldTransform());

            const FTransform ParentWorld = BoneGlobal * ComponentWorld;
            const FTransform Rel = TargetWorld * ParentWorld.Inverse();
            Socket.RelativeLocation = Rel.GetLocation();

            FQuat SafeQuat = Rel.GetRotation();
            SafeQuat.Normalize();
            Socket.RelativeRotation = FRotator(SafeQuat);

            FVector SafeScale = Rel.GetScale3D();
            SafeScale.X = std::max(0.001f, SafeScale.X);
            SafeScale.Y = std::max(0.001f, SafeScale.Y);
            SafeScale.Z = std::max(0.001f, SafeScale.Z);

            Socket.RelativeScale = SafeScale;

            SkelComp->MarkSkinningDirty();
            break;
        }
    }
};
