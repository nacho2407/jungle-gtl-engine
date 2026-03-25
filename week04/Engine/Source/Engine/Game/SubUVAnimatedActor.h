#pragma once
#include "Actor.h"

namespace Engine::Component
{
    class UPrimitiveComponent;
    class USubUVAnimatedComponent;
} // namespace Engine::Component

class ENGINE_API ASubUVAnimatedActor : public AActor
{
    DECLARE_RTTI(ASubUVAnimatedActor, AActor)
  public:
    ASubUVAnimatedActor();
    ~ASubUVAnimatedActor() override = default;

    Engine::Component::USubUVAnimatedComponent* GetSubUVAnimatedComponent() const;

    bool           IsRenderable() const override;
    bool           IsSelected() const override;
    FColor         GetColor() const override;
    EBasicMeshType GetMeshType() const override;
    uint32         GetObjectId() const override;

  private:
    Engine::Component::UPrimitiveComponent* GetPrimitiveComponent() const;
};