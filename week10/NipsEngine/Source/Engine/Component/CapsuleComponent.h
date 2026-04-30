#pragma once
#include "ShapeComponent.h"

class UCapsuleComponent : public UShapeComponent
{
public:
    DECLARE_CLASS(UCapsuleComponent, UShapeComponent)
    float GetCapsuleHalfHeight() const { return CapsuleHalfHeight; }
    float GetCapsuleRadius() const { return CapsuleRadius; }

    float GetScaledCapsuleHalfHeight() const 
	{
        FVector Scale = GetWorldScale();
		return CapsuleHalfHeight * std::abs(Scale.Z); 
	}
    
	float GetScaledCapsuleRadius() const
    {
        FVector Scale = GetWorldScale();
        return CapsuleRadius * std::max(std::max(
                                            std::abs(Scale.X),
                                            std::abs(Scale.Y)),
											std::abs(Scale.Z));
    }

private:
    float CapsuleHalfHeight = 1.0f;
    float CapsuleRadius = 1.0f;

    // UShapeComponent을(를) 통해 상속됨
    void UpdateWorldAABB() const override;
    bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) override;
    EPrimitiveType GetPrimitiveType() const override;
};