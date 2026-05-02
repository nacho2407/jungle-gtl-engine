#pragma once
#include "ShapeComponent.h"
#include "Engine/Collision/CollisionSystem.h"


class UCapsuleComponent : public UShapeComponent
{
public:
    DECLARE_CLASS(UCapsuleComponent, UShapeComponent)
    UCapsuleComponent();

    void Serialize(FArchive& Ar) override;

    virtual FCollision* GetCollision() const override  {   return (FCollision*)&CapsuleCollision; }

    virtual void OnTransformDirty() override;


private:
    float CapsuleHalfHeight;
    float CapsuleRadius;

    FCapsuleCollision CapsuleCollision;
};
