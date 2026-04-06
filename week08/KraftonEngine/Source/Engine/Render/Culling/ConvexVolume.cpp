#include "Engine/Render/Culling/ConvexVolume.h"
#include "Engine/Core/EngineTypes.h"

FPlaneWithMask FConvexVolume::MakePlaneWithMask(const FVector4& InPlane)
{
    FPlaneWithMask Out;
    Out.Plane = InPlane;

    Out.bPX = (Out.Plane.X >= 0.0f);
    Out.bPY = (Out.Plane.Y >= 0.0f);
    Out.bPZ = (Out.Plane.Z >= 0.0f);

    return Out;
}

EAABBResult FConvexVolume::ClassifyAABB(const FBoundingBox& Box) const
{
    bool bAllInside = true;

    for (uint32 i = 0; i < PlaneCount; ++i)
    {
        const FPlaneWithMask& CP = CachedPlanes[i];
        const FVector4& P = CP.Plane;

        const float PX = CP.bPX ? Box.Max.X : Box.Min.X;
        const float PY = CP.bPY ? Box.Max.Y : Box.Min.Y;
        const float PZ = CP.bPZ ? Box.Max.Z : Box.Min.Z;

        if ((P.X * PX) + (P.Y * PY) + (P.Z * PZ) + P.W < 0.0f)
            return EAABBResult::Outside;

        const float NX = CP.bPX ? Box.Min.X : Box.Max.X;
        const float NY = CP.bPY ? Box.Min.Y : Box.Max.Y;
        const float NZ = CP.bPZ ? Box.Min.Z : Box.Max.Z;

        if ((P.X * NX) + (P.Y * NY) + (P.Z * NZ) + P.W < 0.0f)
            bAllInside = false;
    }

    return bAllInside ? EAABBResult::Contains : EAABBResult::Intersects;
}

void FConvexVolume::UpdateFromMatrix(const FMatrix& InViewProjectionMatrix)
{
	const FMatrix& M = InViewProjectionMatrix;

    CachedPlanes[0] = MakePlaneWithMask(FVector4(
        M.M[0][3] + M.M[0][0],
        M.M[1][3] + M.M[1][0],
        M.M[2][3] + M.M[2][0],
        M.M[3][3] + M.M[3][0]
    ));

    CachedPlanes[1] = MakePlaneWithMask(FVector4(
        M.M[0][3] - M.M[0][0],
        M.M[1][3] - M.M[1][0],
        M.M[2][3] - M.M[2][0],
        M.M[3][3] - M.M[3][0]
    ));

    CachedPlanes[2] = MakePlaneWithMask(FVector4(
        M.M[0][3] - M.M[0][1],
        M.M[1][3] - M.M[1][1],
        M.M[2][3] - M.M[2][1],
        M.M[3][3] - M.M[3][1]
    ));

    CachedPlanes[3] = MakePlaneWithMask(FVector4(
        M.M[0][3] + M.M[0][1],
        M.M[1][3] + M.M[1][1],
        M.M[2][3] + M.M[2][1],
        M.M[3][3] + M.M[3][1]
    ));

    CachedPlanes[4] = MakePlaneWithMask(FVector4(
        M.M[0][2],
        M.M[1][2],
        M.M[2][2],
        M.M[3][2]
    ));

    CachedPlanes[5] = MakePlaneWithMask(FVector4(
        M.M[0][3] - M.M[0][2],
        M.M[1][3] - M.M[1][2],
        M.M[2][3] - M.M[2][2],
        M.M[3][3] - M.M[3][2]
    ));
}

bool FConvexVolume::IntersectAABB(const FBoundingBox& Box) const
{
    for (uint32 i = 0; i < PlaneCount; ++i)
    {
        const FPlaneWithMask& CP = CachedPlanes[i];
        const FVector4& P = CP.Plane;

        const float PX = CP.bPX ? Box.Max.X : Box.Min.X;
        const float PY = CP.bPY ? Box.Max.Y : Box.Min.Y;
        const float PZ = CP.bPZ ? Box.Max.Z : Box.Min.Z;

        if ((P.X * PX) + (P.Y * PY) + (P.Z * PZ) + P.W < 0.0f)
            return false;
    }

    return true;
}

bool FConvexVolume::ContainsAABB(const FBoundingBox& Box) const
{
    for (uint32 i = 0; i < PlaneCount; ++i)
    {
        const FPlaneWithMask& CP = CachedPlanes[i];
        const FVector4& P = CP.Plane;

        const float NX = CP.bPX ? Box.Min.X : Box.Max.X;
        const float NY = CP.bPY ? Box.Min.Y : Box.Max.Y;
        const float NZ = CP.bPZ ? Box.Min.Z : Box.Max.Z;

        if ((P.X * NX) + (P.Y * NY) + (P.Z * NZ) + P.W < 0.0f)
            return false;
    }

    return true;
}
