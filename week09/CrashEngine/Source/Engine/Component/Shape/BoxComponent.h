#pragma once
#include "ShapeComponent.h"
#include "Engine/Collision/CollisionSystem.h"

class UBoxComponent : public UShapeComponent
{
public:
    DECLARE_CLASS(UBoxComponent, UShapeComponent)
    UBoxComponent();

    void Serialize(FArchive& Ar) override;
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

    void SetBoxExtent(const FVector& Extent);
    FVector GetBoxExtent() const { return BoxExtent; }

    virtual FCollision* GetCollision() const override { return (FCollision*)&BoxCollision; }
    virtual void OnComponentOverlap(UPrimitiveComponent* Other) const override;

    virtual void OnTransformDirty() override;
    void UpdateWorldAABB() const override;

    FOBB GetOBB() { return BoxCollision.Bounds; }

private:
    FVector BoxExtent;

    FBoxCollision BoxCollision;
};
