#pragma once

#include "Core/Math/Vector.h"

class FD3D11LineBatchRenderer;
struct FEditorRenderData;

class FWorldAxesDrawer
{
  public:
    void Draw(FD3D11LineBatchRenderer& InLineRenderer, const FEditorRenderData& InEditorRenderData);

  private:
    float AxisLength = 1000.0f;
};
