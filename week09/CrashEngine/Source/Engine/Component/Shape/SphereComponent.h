#pragma once
#include "ShapeComponent.h"
#include "Engine/Collision/CollisionSystem.h"


class USphereComponent : public UShapeComponent
{
public:
    DECLARE_CLASS(USphereComponent, UShapeComponent)
    USphereComponent() ;


	void SetRadius(float Radius) { SphereCollision.Sphere.Radius = Radius; }

    virtual FCollision* GetCollision() const override
    {
        return (FCollision*)&SphereCollision; // 내 충돌체를 매니저에게 던져줌
    }
    FShapeProxy* CreateShapeProxy() override;

private:
    float SphereRadius;

	FSphereCollision SphereCollision;
};
