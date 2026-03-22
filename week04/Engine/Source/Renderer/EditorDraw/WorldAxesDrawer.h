#pragma once

class FD3D11LineBatchRenderer;
struct FEditorRenderData;

class FWorldAxesDrawer
{
  public:
    void Draw(FD3D11LineBatchRenderer& InLineRenderer, const FEditorRenderData& InEditorRenderData);

  private:
    float AxisLength = 300.0f;
};
