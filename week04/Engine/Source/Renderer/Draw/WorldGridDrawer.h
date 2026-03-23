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
    int32 GridHalfLineCount = 500;

    // 기존 5.0f보다 넓게
    float GridSpacing = 20.0f;

    // 5칸마다 major line
    int32 MajorLineEvery = 5;

    FColor MinorGridColor = FColor(0.25f, 0.25f, 0.25f, 1.0f);
    FColor MajorGridColor = FColor(0.45f, 0.45f, 0.45f, 1.0f);
};