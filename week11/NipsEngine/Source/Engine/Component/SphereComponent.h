#pragma once
#include "ShapeComponent.h"

class USphereComponent : public UShapeComponent
{
public:
    DECLARE_CLASS(USphereComponent, UShapeComponent)
    float GetSphereRadius() const { return SphereRadius; }
	float GetScaledSphereRadius() const
	{
        FVector Scale = GetWorldScale();
        // Sphere는 방향 영향 없음 → 가장 큰 축 기준으로 보수적으로 처리
        return SphereRadius * std::max(std::max(std::abs(Scale.X), std::abs(Scale.Y)), std::abs(Scale.Z));
	}

private:
    float SphereRadius = 1.0f;

    // UShapeComponent을(를) 통해 상속됨
    void UpdateWorldAABB() const override;
    bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) override;
    EPrimitiveType GetPrimitiveType() const override;
};
