#pragma once

#include "Engine/Game/Actor.h"

namespace Engine::Component
{
    class UPrimitiveComponent;
    class USubUVComponent;
}

class ENGINE_API AEffectActor : public AActor
{
    DECLARE_RTTI(AEffectActor, AActor)

  public:
    AEffectActor();
    ~AEffectActor() override = default;

    Engine::Component::USubUVComponent* GetEffectComponent() const;

    bool           IsRenderable() const override;
    bool           IsSelected() const override;
    FColor         GetColor() const override;
    EBasicMeshType GetMeshType() const override;
    uint32         GetObjectId() const override;

  private:
    Engine::Component::UPrimitiveComponent* GetPrimitiveComponent() const;
};
