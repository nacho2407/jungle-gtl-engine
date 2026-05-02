#pragma once
#include "ShapeComponent.h"
#include "Engine/Collision/CollisionSystem.h"


class USphereComponent : public UShapeComponent
{
public:
    DECLARE_CLASS(USphereComponent, UShapeComponent)
    USphereComponent();

    void Serialize(FArchive& Ar) override;


    void SetRadius(float Radius) { SphereCollision.Sphere.Radius = Radius; }

    virtual FCollision* GetCollision() const override { return (FCollision*)&SphereCollision; }

    virtual void OnTransformDirty() override;

private:
    float SphereRadius;

    FSphereCollision SphereCollision;
};
