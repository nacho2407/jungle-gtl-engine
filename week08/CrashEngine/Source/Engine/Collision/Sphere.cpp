#include "Sphere.h"

bool FSphere::IntersectAABB(const FBoundingBox& Box) const
{
    float S = 0.0f;
    float D = Radius * Radius;

    if (Center.X < Box.Min.X) S += (Center.X - Box.Min.X) * (Center.X - Box.Min.X);
    else if (Center.X > Box.Max.X) S += (Center.X - Box.Max.X) * (Center.X - Box.Max.X);

    if (Center.Y < Box.Min.Y) S += (Center.Y - Box.Min.Y) * (Center.Y - Box.Min.Y);
    else if (Center.Y > Box.Max.Y) S += (Center.Y - Box.Max.Y) * (Center.Y - Box.Max.Y);

    if (Center.Z < Box.Min.Z) S += (Center.Z - Box.Min.Z) * (Center.Z - Box.Min.Z);
    else if (Center.Z > Box.Max.Z) S += (Center.Z - Box.Max.Z) * (Center.Z - Box.Max.Z);

    return S <= D;
}