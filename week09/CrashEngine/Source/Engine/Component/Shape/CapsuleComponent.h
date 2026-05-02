#pragma once
#include "ShapeComponent.h"
#include "Engine/Collision/CollisionSystem.h"


class UCapsuleComponent : public UShapeComponent
{
public:
    DECLARE_CLASS(UCapsuleComponent, UShapeComponent)
    UCapsuleComponent();

    void Serialize(FArchive& Ar) override;
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

    virtual void OnComponentOverlap(UPrimitiveComponent* Other) const override;

    virtual FCollision* GetCollision() const override { return (FCollision*)&CapsuleCollision; }

    virtual void OnTransformDirty() override;


private:
    float CapsuleHalfHeight;
    float CapsuleRadius;

    FCapsuleCollision CapsuleCollision;
};
