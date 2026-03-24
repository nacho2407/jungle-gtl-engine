#pragma once
#include "Actor.h"

namespace Engine::Component
{
    class UPrimitiveComponent;
    class UConeComponent;
}

class ENGINE_API AConeActor : public AActor
{
    DECLARE_RTTI(AConeActor, AActor)
    
public:
    AConeActor();
    ~AConeActor() override = default;
    
    Engine::Component::UConeComponent * GetConeComponent() const;
    
    bool           IsRenderable() const override;
    bool           IsSelected() const override;
    FColor         GetColor() const override;
    EBasicMeshType GetMeshType() const override;
    uint32         GetObjectId() const override;
    
private:
    Engine::Component::UPrimitiveComponent* GetPrimitiveComponent() const;
};
