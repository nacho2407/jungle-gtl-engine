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

	void     SetFogDensity(float InFogDensity) { FogDensity = InFogDensity; }
	float GetFogDensity() const { return FogDensity; }

	void     SetHeightFalloff(float InHeightFalloff) { HeightFalloff = InHeightFalloff; }
    float GetHeightFalloff() const { return HeightFalloff; }

	void     SetFogInscatteringColor(const FVector4& InColor) { FogInscatteringColor = InColor; }
    FVector4 GetFogInscatteringColor() const { return FogInscatteringColor; }

  private:
    FVector4 FogInscatteringColor;
    float FogDensity;
	float HeightFalloff;

    // UPrimitiveComponent을(를) 통해 상속됨
    void UpdateWorldAABB() const override;
    bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) override;
};