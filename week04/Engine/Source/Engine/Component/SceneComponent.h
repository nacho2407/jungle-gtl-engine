#pragma once

#include "Core/CoreMinimal.h"
#include "CoreUObject/Object.h"

namespace Engine::Component
{
    class ENGINE_API USceneComponent : public UObject
    {
        DECLARE_RTTI(USceneComponent, UObject)
      public:
        USceneComponent() = default;
        virtual ~USceneComponent() override = default;

      public:
        FVector  GetRelativeLocation() const { return WorldTransform.GetLocation(); }
        FRotator GetRelativeRotation() const { return WorldTransform.Rotator(); }
        FVector  GetRelativeScale3D() const { return WorldTransform.GetScale3D(); }
        FQuat    GetRelativeQuaternion() const { return WorldTransform.GetRotation(); }

        virtual void SetRelativeLocation(const FVector& NewLocation);
        virtual void SetRelativeRotation(const FQuat& NewRotation);
        virtual void SetRelativeRotation(const FRotator& NewRotation);
        virtual void SetRelativeScale3D(const FVector& NewScale);

        virtual void Update(float DeltaTime);

        FMatrix GetRelativeMatrix() const;

        virtual void DrawProperties();

        bool IsSelected() const { return bIsSelected; }

      protected:
        virtual void OnTransformChanged() {}

      protected:
        bool bIsSelected = false;

      private:
        FTransform WorldTransform;
    };
} // namespace Engine::Component