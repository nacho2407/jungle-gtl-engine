#pragma once

#include "Engine/Game/Actor.h"

namespace Engine::Component
{
    class UPrimitiveComponent;
    class USpriteComponent;
}

class ENGINE_API ASpriteActor : public AActor
{
    DECLARE_RTTI(ASpriteActor, AActor)

  public:
    ASpriteActor();
    ~ASpriteActor() override = default;

    Engine::Component::USpriteComponent* GetSpriteComponent() const;

    bool           IsRenderable() const override;
    bool           IsSelected() const override;
    FColor         GetColor() const override;
    EBasicMeshType GetMeshType() const override;
    uint32         GetObjectId() const override;

  private:
    Engine::Component::UPrimitiveComponent* GetPrimitiveComponent() const;
};
