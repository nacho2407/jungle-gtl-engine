#pragma once

#include "Core/HAL/PlatformTypes.h"
#include "Core/Math/Color.h"

class FD3D11LineBatchRenderer;
struct FEditorRenderData;

class FWorldGridDrawer
{
  public:
    void Draw(FD3D11LineBatchRenderer& InLineRenderer, const FEditorRenderData& InEditorRenderData);

  private:
    int32  GridHalfLineCount = 50;
    float  GridSpacing = 100.0f;
    FColor GridColor = FColor(0.25f, 0.25f, 0.25f, 1.0f);
};
