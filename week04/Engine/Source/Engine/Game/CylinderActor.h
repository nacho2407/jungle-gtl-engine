#pragma once
#include "Actor.h"

namespace Engine::Component
{
    class UCylinderComponent;
    class UPrimitiveComponent;
}

class ENGINE_API ACylinderActor : public AActor
{
    DECLARE_RTTI(ACylinderActor, AActor)
    
public:
    ACylinderActor();
    ~ACylinderActor() override = default;
    
    Engine::Component::UCylinderComponent * GetCylinderComponent() const;
    
    bool           IsRenderable() const override;
    bool           IsSelected() const override;
    FColor         GetColor() const override;
    EBasicMeshType GetMeshType() const override;
    uint32         GetObjectId() const override;
    
private:
    Engine::Component::UPrimitiveComponent* GetPrimitiveComponent() const;
};
