#pragma once

#include "Core/CoreMinimal.h"
#include "CoreUObject/Object.h"

namespace Engine::Component
{
    class ENGINE_API USceneComponent : public UObject
    {
        DECLARE_RTTI(USceneComponent, UObject)
      public:
        USceneComponent();
        virtual ~USceneComponent() override;

      public:
        inline const FVector  GetRelativeLocation() { return WorldTransform.GetLocation(); }
        inline const FRotator GetRelativeRotation() { return WorldTransform.Rotator(); }
        inline const FVector  GetRelativeScale3D() { return WorldTransform.GetScale3D(); }
        inline const FQuat    GetRelativeQuaternion() { return WorldTransform.GetRotation(); }

        inline virtual void   SetRelativeLocation(const FVector& NewLocation)
        {
            WorldTransform.SetLocation(NewLocation);
        }
        inline virtual void SetRelativeRotation(const FQuat& NewRotation)
        {
            WorldTransform.SetRotation(NewRotation);
        }
        inline virtual void SetRelativeRotation(const FRotator& NewRotation)
        {
            WorldTransform.SetRotation(NewRotation);
        }
        inline virtual void SetRelativeScale3D(const FVector& NewScale)
        {
            WorldTransform.SetScale3D(NewScale);
        }

        FMatrix GetRelativeMatrix();

        virtual void OnComponentAdded() {};
        virtual void Update(float deltaTime) {};
        virtual void DrawProperties();

        bool IsSelected() const;

        bool            bIsSelected = false;
        Geometry::FAABB AABB;

      private:
        FTransform WorldTransform;
    };
} // namespace Engine::Component
