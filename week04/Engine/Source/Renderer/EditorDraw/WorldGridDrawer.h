#pragma once

#include "Core/HAL/PlatformTypes.h"
#include "Core/Math/Color.h"
#include "Core/Math/Vector.h"

class FD3D11LineBatchRenderer;
struct FEditorRenderData;

class FWorldGridDrawer
{
  public:
    void Draw(FD3D11LineBatchRenderer& InLineRenderer, const FEditorRenderData& InEditorRenderData);

  private:
    int32  GridHalfLineCount = 500;
    float  GridSpacing = 5.0f;
    FColor GridColor = FColor(0.25f, 0.25f, 0.25f, 1.0f);
};
