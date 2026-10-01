#pragma once
#include "Core/EngineTypes.h"
#include "Math/Vector.h"

class FSphere
{
public:
    FVector Center;
    float Radius;
    
    bool IntersectAABB(const FBoundingBox& Box) const;
};
