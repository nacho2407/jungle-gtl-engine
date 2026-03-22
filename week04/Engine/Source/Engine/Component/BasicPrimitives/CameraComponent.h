#include <Core/CoreMinimal.h>
#include "Engine/Component/SceneComponent.h"

namespace Engine::Component
{
    enum class EProjectionType
    {
        Perspective,
        Orthographic
    };

    class ENGINE_API UCameraComponent : public USceneComponent
    {
        DECLARE_RTTI(UCameraComponent, USceneComponent)

      public:
        UCameraComponent() = default;
        virtual ~UCameraComponent() override = default;

        inline void SetRelativeLocation(const FVector& NewLocation) override;
        inline void SetRelativeRotation(const FQuat& NewRotation) override;
        inline void SetRelativeRotation(const FRotator& NewRotation) override;

        virtual void Update(float InDeltaTime) override;

        FMatrix GetViewMatrix() const;
        FMatrix GetProjectionMatrix() const;
        FMatrix GetViewProjectionMatrix() const;

        void SetProjectionType(EProjectionType InType);
        void SetFOV(float InFOV);
        void SetNearPlane(float InNear);
        void SetFarPlane(float InFar);
        void SetOrthoHeight(float InHeight);

        void OnResize(uint32 InWidth, uint32 InHeight);

        FVector WorldToScreen(const FVector& InWorldPos) const;

      private:
        EProjectionType ProjectionType = EProjectionType::Perspective;
        int32           Height{1080};
        int32           Width{1920};
        float           AspectRatio{16.0f / 9.0f};
        float           FOV{90.0f};
        float           NearPlane{0.1f};
        float           FarPlane{2000.0f};
        float           OrthoHeight{30.0f};

        mutable FMatrix CachedViewMatrix;
        mutable FMatrix CachedProjectionMatrix;
        mutable bool    bIsViewDirty = true;
        mutable bool    bIsProjectionDirty = true;
        // bool            bIsReverseZ = false;
    };
} // namespace Engine::Component
