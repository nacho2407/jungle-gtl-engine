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

    void SetCapsuleSize(float Radius, float HalfHeight);
    void SetCapsuleRadius(float Radius);
    void SetCapsuleHalfHeight(float HalfHeight);
    float GetCapsuleRadius() const { return CapsuleRadius; }
    float GetCapsuleHalfHeight() const { return CapsuleHalfHeight; }

    virtual void OnComponentOverlap(UPrimitiveComponent* Other) const override;

    virtual FCollision* GetCollision() const override { return (FCollision*)&CapsuleCollision; }

    virtual void OnTransformDirty() override;
    void UpdateWorldAABB() const override;


private:
    float CapsuleHalfHeight;
    float CapsuleRadius;

    FCapsuleCollision CapsuleCollision;
};
