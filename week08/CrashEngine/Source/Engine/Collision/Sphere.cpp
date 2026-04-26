#include "Sphere.h"

#include "Math/Intersection.h"

bool FSphere::IntersectAABB(const FBoundingBox& Box) const
{
    return FMath::IntersectSphereAABB(Center, Radius, Box);
}
