#pragma once
#include "ShapeComponent.h"

class UCapsuleComponent : public UShapeComponent
{
public:
    DECLARE_CLASS(UCapsuleComponent, UShapeComponent)
    float GetCapsuleHalfHeight() const { return CapsuleHalfHeight; }
    float GetCapsuleRadius() const { return CapsuleRadius; }

	void UpdateWorldAABB() const override;

    float GetScaledCapsuleHalfHeight() const 
	{
        FVector Axis = GetWorldTransform().GetUnitAxis(EAxis::Z);
        FVector Scale = GetWorldScale();
        float UpAxisScale = std::sqrt(
            Axis.X * Axis.X * Scale.X * Scale.X +
            Axis.Y * Axis.Y * Scale.Y * Scale.Y +
            Axis.Z * Axis.Z * Scale.Z * Scale.Z);
		return CapsuleHalfHeight * UpAxisScale; 
	}
    
	float GetScaledCapsuleRadius() const
    {
        FVector Axis = GetWorldTransform().GetUnitAxis(EAxis::Z);
        FVector Scale = GetWorldScale();

		// Capsule Axis = Z 일 때, X/Y 만 보면 됨
        return CapsuleRadius * std::max(std::abs(Scale.X), std::abs(Scale.Y));
    }

    void PostDuplicate(UObject* Original) override;
    void Serialize(FArchive& Ar) override;

private:
    float CapsuleHalfHeight = 0.5f;
    float CapsuleRadius = 0.5f;

    bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) override;
    EPrimitiveType GetPrimitiveType() const override;
};