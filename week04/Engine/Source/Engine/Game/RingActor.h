#pragma once
#include "Actor.h"

namespace Engine::Component
{
    class URingComponent;
    class UPrimitiveComponent;
    class UConeComponent;
}

class ENGINE_API ARingActor : public AActor
{
    DECLARE_RTTI(ARingActor, AActor)
    
public:
    ARingActor();
    ~ARingActor() override = default;
    
    Engine::Component::URingComponent * GetRingComponent() const;
    
    bool           IsRenderable() const override;
    bool           IsSelected() const override;
    FColor         GetColor() const override;
    EBasicMeshType GetMeshType() const override;
    uint32         GetObjectId() const override;
    
private:
    Engine::Component::UPrimitiveComponent* GetPrimitiveComponent() const;
};
