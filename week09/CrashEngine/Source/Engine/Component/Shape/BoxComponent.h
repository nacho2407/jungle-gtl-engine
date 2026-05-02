#pragma once
#include "ShapeComponent.h"
#include "Engine/Collision/CollisionSystem.h"

class UBoxComponent : public UShapeComponent
{
public:
    DECLARE_CLASS(UBoxComponent, UShapeComponent)
    UBoxComponent();

    // void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    // void PostEditProperty(const char* PropertyName) override;

    void Serialize(FArchive& Ar) override;

    virtual FCollision* GetCollision() const override { return (FCollision*)&BoxCollision; }

    virtual void OnTransformDirty() override;

    FOBB GetOBB() { return BoxCollision.Bounds; }

private:
    FVector BoxExtent;

    FBoxCollision BoxCollision;
};
