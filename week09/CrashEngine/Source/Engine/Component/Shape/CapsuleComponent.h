#pragma once
#include "ShapeComponent.h"
#include "Engine/Collision/CollisionSystem.h"


class UCapsuleComponent : public UShapeComponent
{
public:
    DECLARE_CLASS(UBoxComponent, UShapeComponent)
    UCapsuleComponent();

    virtual FCollision* GetCollision() const override
    {
        return (FCollision*)&CapsuleCollision; // 내 충돌체를 매니저에게 던져줌
    }

	FShapeProxy* CreateShapeProxy() override;

private:
    float CapsuleHalfHeight;
    float CapsuleRadius;

    FCapsuleCollision CapsuleCollision;
};
