#pragma once
#include "Actor.h"

namespace Engine::Component
{
    class UPrimitiveComponent;
    class USpriteComponent;
}

class ENGINE_API ASpriteTextureActor : public AActor
{
    DECLARE_RTTI(AConeActor, AActor)
public:
    ASpriteTextureActor();
    ~ASpriteTextureActor() override = default;
    
    Engine::Component::USpriteComponent * GetSpriteTextureComponent() const;
    
    bool           IsRenderable() const override;
    bool           IsSelected() const override;
    FColor         GetColor() const override;
    EBasicMeshType GetMeshType() const override;
    uint32         GetObjectId() const override;
    
private:
    Engine::Component::UPrimitiveComponent* GetPrimitiveComponent() const;
};
