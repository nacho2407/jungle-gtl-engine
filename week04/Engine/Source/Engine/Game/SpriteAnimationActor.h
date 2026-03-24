#pragma once
#include "Actor.h"

namespace Engine::Component
{
    class UPrimitiveComponent;
    class UAnimatedSpriteComponent;
}

class ENGINE_API ASpriteAnimationActor : public AActor
{
    DECLARE_RTTI(AConeActor, AActor)
public:
    ASpriteAnimationActor();
    ~ASpriteAnimationActor() override = default;
    
    Engine::Component::UAnimatedSpriteComponent * GetAnimatedSpriteComponent() const;
    
    bool           IsRenderable() const override;
    bool           IsSelected() const override;
    FColor         GetColor() const override;
    EBasicMeshType GetMeshType() const override;
    uint32         GetObjectId() const override;
    
private:
    Engine::Component::UPrimitiveComponent* GetPrimitiveComponent() const;
};
