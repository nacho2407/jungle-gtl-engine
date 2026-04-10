#pragma once
#include "PrimitiveComponent.h"

class UHeightFogComponent : public UPrimitiveComponent
{
public:
    DECLARE_CLASS(UHeightFogComponent, UPrimitiveComponent)

	UHeightFogComponent();
    ~UHeightFogComponent() override = default;

	virtual UHeightFogComponent* Duplicate() override;
    virtual UHeightFogComponent* DuplicateSubObjects() override { return this; }

    EPrimitiveType               GetPrimitiveType() const override { return EPrimitiveType::EPT_FOG; }

	float GetFogDensity() const { return FogDensity; }
    float GetHeightFalloff() const { return HeightFalloff; }
    FVector4 GetFogInscatteringColor() const { return FogInscatteringColor; }

  private:
    float FogDensity;
	float HeightFalloff;
    FVector4 FogInscatteringColor;

        // UPrimitiveComponent을(를) 통해 상속됨
        void UpdateWorldAABB() const override;
    bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) override;
};