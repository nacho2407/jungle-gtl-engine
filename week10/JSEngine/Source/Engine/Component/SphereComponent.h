#pragma once
#include "ShapeComponent.h"

class USphereComponent : public UShapeComponent
{
public:
    DECLARE_CLASS(USphereComponent, UShapeComponent)
    float GetSphereRadius() const { return SphereRadius; }
	float GetScaledSphereRadius() const
	{
        return SphereRadius;
	}

    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostDuplicate(UObject* Original) override;
    void Serialize(FArchive& Ar) override;

private:
    float SphereRadius = 0.5f;

    // UShapeComponent을(를) 통해 상속됨
    void UpdateWorldAABB() const override;
    bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) override;
    EPrimitiveType GetPrimitiveType() const override;
};
