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

    void USceneComponent::SetRelativeLocation(const FVector& NewLocation)
    {
        if (WorldTransform.GetLocation() == NewLocation)
        {
            return;
        }

        WorldTransform.SetLocation(NewLocation);
        OnTransformChanged();
    }

    void USceneComponent::SetRelativeRotation(const FQuat& NewRotation)
    {
        if (WorldTransform.GetRotation() == NewRotation)
        {
            return;
        }

        WorldTransform.SetRotation(NewRotation);
        OnTransformChanged();
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
    }

    void USceneComponent::SetRelativeTransform(const FVector& NewTransform)
    {
        WorldTransform.SetScale3D(NewTransform);
        OnTransformChanged();
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

    bool USceneComponent::IsSelected() const { return bIsSelected; }
    void USceneComponent::SetSelected(bool bInSelected) { bIsSelected = bInSelected; }
    
} // namespace Engine::Component
