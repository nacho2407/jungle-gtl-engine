#include "SceneComponent.h"

#include "Core/Geometry/Primitives/AABBUtility.h"

#include <algorithm>

namespace Engine::Component
{
    USceneComponent::~USceneComponent()
    {
        DetachFromParent();

        for (USceneComponent* ChildComponent : AttachChildren)
        {
            if (ChildComponent != nullptr)
            {
                ChildComponent->AttachParent = nullptr;
            }
        }

        AttachChildren.clear();
        OwnerActor = nullptr;
    }

    void USceneComponent::Serialize(bool bIsLoading, void* JsonHandle)
    {
        // 최상위 컴포넌트이므로 부모 호출(UObject::Serialize 등)은 생략하거나
        // 나중에 필요한 공통 데이터가 생기면 이곳에 추가합니다.
        (void)bIsLoading;
        (void)JsonHandle;
    }

    void USceneComponent::SetRelativeLocation(const FVector& NewLocation)
    {
        if (WorldTransform.GetLocation() == NewLocation)
        {
            return;
        }

        WorldTransform.SetLocation(NewLocation);
        OnTransformChanged();
        PropagateTransformChanged();
    }

    void USceneComponent::SetRelativeRotation(const FQuat& NewRotation)
    {
        if (WorldTransform.GetRotation() == NewRotation)
        {
            return;
        }

        WorldTransform.SetRotation(NewRotation);
        OnTransformChanged();
        PropagateTransformChanged();
    }

    void USceneComponent::SetRelativeRotation(const FRotator& NewRotation)
    {
        SetRelativeRotation(NewRotation.Quaternion());
    }

    void USceneComponent::SetRelativeScale3D(const FVector& NewScale)
    {
        if (WorldTransform.GetScale3D() == NewScale)
        {
            return;
        }
        FVector AbsScale;
        AbsScale.X = std::max(NewScale.X, 0.0001f);
        AbsScale.Y = std::max(NewScale.Y, 0.0001f);
        AbsScale.Z = std::max(NewScale.Z, 0.0001f);
        WorldTransform.SetScale3D(AbsScale);
        
        OnTransformChanged();
        PropagateTransformChanged();
    }

    void USceneComponent::SetRelativeTransform(const FVector& NewTransform)
    {
        WorldTransform.SetScale3D(NewTransform);
        OnTransformChanged();
        PropagateTransformChanged();
    }

    void USceneComponent::SetOwnerActor(AActor* InOwnerActor)
    {
        OwnerActor = InOwnerActor;

        for (USceneComponent* ChildComponent : AttachChildren)
        {
            if (ChildComponent != nullptr)
            {
                ChildComponent->SetOwnerActor(InOwnerActor);
            }
        }
    }

    void USceneComponent::AttachToComponent(USceneComponent* InParent)
    {
        if (InParent == this)
        {
            return;
        }

        if (InParent != nullptr)
        {
            if (OwnerActor != nullptr && InParent->GetOwnerActor() != nullptr &&
                OwnerActor != InParent->GetOwnerActor())
            {
                return;
            }

            for (const USceneComponent* ParentIterator = InParent; ParentIterator != nullptr;
                 ParentIterator = ParentIterator->GetAttachParent())
            {
                if (ParentIterator == this)
                {
                    return;
                }
            }
        }

        if (AttachParent == InParent)
        {
            return;
        }

        DetachFromParent();

        AttachParent = InParent;
        if (AttachParent != nullptr)
        {
            if (OwnerActor == nullptr)
            {
                SetOwnerActor(AttachParent->GetOwnerActor());
            }

            AttachParent->AttachChildren.push_back(this);
        }
    }

    void USceneComponent::DetachFromParent()
    {
        if (AttachParent == nullptr)
        {
            return;
        }

        TArray<USceneComponent*>& SiblingArray = AttachParent->AttachChildren;
        const auto NewEnd =
            std::remove(SiblingArray.begin(), SiblingArray.end(), this);
        if (NewEnd != SiblingArray.end())
        {
            SiblingArray.erase(NewEnd, SiblingArray.end());
        }

        AttachParent = nullptr;
    }

    void USceneComponent::PropagateTransformChanged()
    {
        for (USceneComponent* ChildComponent : AttachChildren)
        {
            if (ChildComponent != nullptr)
            {
                ChildComponent->OnTransformChanged();
                ChildComponent->PropagateTransformChanged();
            }
        }
    }

    void USceneComponent::Update(float DeltaTime) {}

    void USceneComponent::DescribeProperties(FComponentPropertyBuilder& Builder)
    {
        (void)Builder;
    }

    void USceneComponent::ResolveAssetReferences(UAssetManager* InAssetManager)
    {
        (void)InAssetManager;
    }

    FMatrix USceneComponent::GetRelativeMatrix() const
    {
        return WorldTransform.ToMatrix();
    }

    FMatrix USceneComponent::GetRelativeMatrixNoScale() const
    {
        return WorldTransform.ToMatrixNoScale();
    }

    FMatrix USceneComponent::GetWorldMatrix() const
    {
        if (AttachParent == nullptr)
        {
            return GetRelativeMatrix();
        }

        return GetRelativeMatrix() * AttachParent->GetWorldMatrix();
    }

    FMatrix USceneComponent::GetWorldMatrixNoScale() const
    {
        if (AttachParent == nullptr)
        {
            return GetRelativeMatrixNoScale();
        }

        return GetRelativeMatrixNoScale() * AttachParent->GetWorldMatrixNoScale();
    }

    bool USceneComponent::IsSelected() const { return bIsSelected; }
    void USceneComponent::SetSelected(bool bInSelected) { bIsSelected = bInSelected; }
    
} // namespace Engine::Component
