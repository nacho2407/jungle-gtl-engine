#pragma once

#include "Core/CoreMinimal.h"
#include "CoreUObject/Object.h"

#include "Core/Logging/LogMacros.h"

class AActor;
class UAssetManager;

namespace Engine::Component
{
    class FComponentPropertyBuilder;

    class ENGINE_API USceneComponent : public UObject
    {
        DECLARE_RTTI(USceneComponent, UObject)

    public:
        USceneComponent() = default;
        virtual ~USceneComponent() override;

    public:
        FVector GetRelativeLocation() const { return WorldTransform.GetLocation(); }
        FRotator GetRelativeRotation() const { return WorldTransform.Rotator(); }
        FVector  GetRelativeScale3D() const { return WorldTransform.GetScale3D(); }
        FQuat    GetRelativeQuaternion() const { return WorldTransform.GetRotation(); }
        FTransform GetRelativeTransform() const { return WorldTransform; }

        AActor* GetOwnerActor() const { return OwnerActor; }
        USceneComponent* GetAttachParent() const { return AttachParent; }
        const TArray<USceneComponent*>& GetAttachChildren() const { return AttachChildren; }

        virtual void SetRelativeLocation(const FVector& NewLocation);
        virtual void SetRelativeRotation(const FQuat& NewRotation);
        virtual void SetRelativeRotation(const FRotator& NewRotation);
        virtual void SetRelativeScale3D(const FVector& NewScale);
        virtual void SetRelativeTransform(const FVector& NewTransform);

        void         SetOwnerActor(AActor* InOwnerActor);
        void         AttachToComponent(USceneComponent* InParent);
        void         DetachFromParent();

        virtual void Update(float DeltaTime);
        virtual void DescribeProperties(FComponentPropertyBuilder& Builder);
        virtual void ResolveAssetReferences(UAssetManager* InAssetManager);

        FMatrix GetRelativeMatrix() const;
        FMatrix GetRelativeMatrixNoScale() const;

        bool IsSelected() const;
        void SetSelected(bool bInSelected);

        virtual bool IsShowBounds() const { return false; };

        virtual void SetShowBounds(bool bInShowBounds)
        {
            // Do nothing
        };

    protected:
        virtual void OnTransformChanged()
        {
            UE_LOG(USceneComponent, ELogVerbosity::Warning, "OMG This it not true");
        }

    protected:
        bool bIsSelected = false;

    protected:
        FTransform               WorldTransform;
        AActor*                  OwnerActor = nullptr;
        USceneComponent*         AttachParent = nullptr;
        TArray<USceneComponent*> AttachChildren;
    };
} // namespace Engine::Component
