#pragma once
#include "ShapeComponent.h"
#include "Engine/Collision/CollisionSystem.h"


class USphereComponent : public UShapeComponent
{
public:
    DECLARE_CLASS(USphereComponent, UShapeComponent)
    USphereComponent();

    void Serialize(FArchive& Ar) override;
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

    void SetRadius(float Radius);
    float GetRadius() const { return SphereRadius; }

    virtual void OnComponentOverlap(UPrimitiveComponent* Other) const override;

    virtual FCollision* GetCollision() const override { return (FCollision*)&SphereCollision; }

    virtual void OnTransformDirty() override;
    void UpdateWorldAABB() const override;

private:
    float SphereRadius;

    FSphereCollision SphereCollision;
};
