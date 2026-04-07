#include "Frustum.h"

#include <cmath>
#include <cassert>

void FFrustum::UpdateFromCamera(const FMatrix& View, const FMatrix& Projection)
{
    UpdateFromCamera(View * Projection);
}

void FFrustum::UpdateFromCamera(const FMatrix& ViewProjection)
{
    const float(&M)[4][4] = ViewProjection.M;

    const XMVector C0 = DirectX::XMVectorSet(M[0][0], M[1][0], M[2][0], M[3][0]);
    const XMVector C1 = DirectX::XMVectorSet(M[0][1], M[1][1], M[2][1], M[3][1]);
    const XMVector C2 = DirectX::XMVectorSet(M[0][2], M[1][2], M[2][2], M[3][2]);
    const XMVector C3 = DirectX::XMVectorSet(M[0][3], M[1][3], M[2][3], M[3][3]);

    Float4 P0, P1, P2, P3, P4, P5;

    // Row-vector clip tests use matrix columns:
    // x_c = dot(v, c0), y_c = dot(v, c1), z_c = dot(v, c2), w_c = dot(v, c3)
    // D3D reverse-Z depth range is still [0, 1], but near/far swap:
    // Left   : x_c + w_c >= 0  -> c0 + c3
    // Right  : w_c - x_c >= 0  -> c3 - c0
    // Bottom : y_c + w_c >= 0  -> c1 + c3
    // Top    : w_c - y_c >= 0  -> c3 - c1
    // Near   : w_c - z_c >= 0  -> c3 - c2
    // Far    : z_c >= 0        -> c2
    DirectX::XMStoreFloat4(&P0, DirectX::XMVectorAdd(C0, C3));
    DirectX::XMStoreFloat4(&P1, DirectX::XMVectorSubtract(C3, C0));
    DirectX::XMStoreFloat4(&P2, DirectX::XMVectorAdd(C1, C3));
    DirectX::XMStoreFloat4(&P3, DirectX::XMVectorSubtract(C3, C1));
    DirectX::XMStoreFloat4(&P4, DirectX::XMVectorSubtract(C3, C2));
    DirectX::XMStoreFloat4(&P5, C2);

    Planes[0] = FPlane(FVector(P0.x, P0.y, P0.z), P0.w);
    Planes[1] = FPlane(FVector(P1.x, P1.y, P1.z), P1.w);
    Planes[2] = FPlane(FVector(P2.x, P2.y, P2.z), P2.w);
    Planes[3] = FPlane(FVector(P3.x, P3.y, P3.z), P3.w);
    Planes[4] = FPlane(FVector(P4.x, P4.y, P4.z), P4.w);
    Planes[5] = FPlane(FVector(P5.x, P5.y, P5.z), P5.w);

    for (FPlane& Plane : Planes)
    {
        Plane.Normalize(MathUtil::Epsilon);
    }

    const FMatrix InverseViewProjection = ViewProjection.GetInverse();
    const FVector TestPoint =
        InverseViewProjection.TransformPosition(FVector(0.0f, 0.0f, 0.5f));

    // Safety net: if conventions drift, make sure all planes still face inward.
    {
        for (FPlane& Plane : Planes)
        {
            const float Distance = Plane.GetSignedDistanceToPoint(TestPoint);
            if (Distance < 0.0f)
            {
                Plane.Flip();
            }
        }
    }

#if defined (_DEBUG)
    // Debug check: a point in the middle of the frustum (NDC z=0.5) should be inside all planes.
    {
        for (const FPlane& Plane : Planes)
        {
            const float Distance = Plane.GetSignedDistanceToPoint(TestPoint);
            assert(Distance >= -MathUtil::Epsilon);
        }
    }
#endif
}

FFrustum::EFrustumIntersectResult FFrustum::Intersects(const FAABB& Box) const
{
    const FVector Center = Box.GetCenter();
    const FVector Extent = Box.GetExtent();
    bool bAllInside = true;

    for (const FPlane& Plane : Planes)
    {
        const float Radius = Plane.AbsNormal.X * Extent.X + Plane.AbsNormal.Y * Extent.Y +
                             Plane.AbsNormal.Z * Extent.Z;

        const float Distance = Plane.GetSignedDistanceToPoint(Center);
        if (Distance + Radius < 0.0f)
        {
            return EFrustumIntersectResult::Outside;
        }

        if (Distance - Radius < 0.0f)
        {
            bAllInside = false;
        }
    }
    return bAllInside ? EFrustumIntersectResult::Inside : EFrustumIntersectResult::Intersect;
}

bool FFrustum::Contains(const FVector& Point) const
{
    for (const FPlane& Plane : Planes)
    {
        if (Plane.GetSignedDistanceToPoint(Point) < 0.0f)
        {
            return false;
        }
    }
    return true;
}
