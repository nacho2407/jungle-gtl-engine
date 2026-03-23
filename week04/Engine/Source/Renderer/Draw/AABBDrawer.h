#pragma once

#include "Core/Geometry/Primitives/AABB.h"
#include "Core/Math/Color.h"

class FD3D11LineBatchRenderer;

class FAABBDrawer
{
  public:
    void Draw(FD3D11LineBatchRenderer& InLineRenderer, const Geometry::FAABB& InBounds,
              const FColor& InColor) const;
};
