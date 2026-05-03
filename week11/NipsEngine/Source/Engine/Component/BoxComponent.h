#pragma once
#include "ShapeComponent.h"
#include "Geometry/OBB.h"

class UBoxComponent : public UShapeComponent
{
public:
    DECLARE_CLASS(UBoxComponent, UShapeComponent)
    void UpdateWorldAABB() const override
    {
        const FTransform& T = GetWorldTransform();

        FVector Center = T.GetLocation();
        FVector Scale = T.GetScale3D();

        FVector HalfExtent = (Extent * 0.5f) * Scale;

        // Rotation까지 포함한 conservative AABB 계산
        const FMatrix R = T.GetRotation().ToMatrix();

        FVector AbsExtent(
            std::abs(R.M[0][0]) * HalfExtent.X + std::abs(R.M[1][0]) * HalfExtent.Y + std::abs(R.M[2][0]) * HalfExtent.Z,
            std::abs(R.M[0][1]) * HalfExtent.X + std::abs(R.M[1][1]) * HalfExtent.Y + std::abs(R.M[2][1]) * HalfExtent.Z,
            std::abs(R.M[0][2]) * HalfExtent.X + std::abs(R.M[1][2]) * HalfExtent.Y + std::abs(R.M[2][2]) * HalfExtent.Z);

        WorldAABB.Min = Center - AbsExtent;
        WorldAABB.Max = Center + AbsExtent;
    }

	FOBB GetWorldOBB() const
	{
        const FTransform& T = GetWorldTransform();
        return FOBB(GetWorldLocation(), Extent * 0.5f * T.GetScale3D(), T.GetRotation().ToMatrix());
	}
		
    void PostDuplicate(UObject* Original) override;
    void Serialize(FArchive& Ar) override;

private:
    FVector Extent = FVector(1, 1, 1);

    // UShapeComponent을(를) 통해 상속됨
    bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) override;
    EPrimitiveType GetPrimitiveType() const override;
};