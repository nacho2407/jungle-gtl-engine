#pragma once

#include "Renderer/SceneRenderData.h"

class FD3D11LineBatchRenderer;
class FSceneView;
class FFontResource;
struct FTextRenderItem;
struct FSpriteRenderItem;
struct FPrimitiveRenderItem;

class FAABBSubmitter
{
  public:
    void Submit(FD3D11LineBatchRenderer& InLineRenderer,
                const FSceneRenderData&   InSceneRenderData) const;

  private:
    static FColor ResolveBoundsColor(const FRenderItemState& InState);
    static void   ExpandBounds(FVector& InOutMin, FVector& InOutMax, const FVector& InPoint);
    static void   SubmitBox(FD3D11LineBatchRenderer& InLineRenderer, const FVector& InMin,
                            const FVector& InMax, const FColor& InColor);

    static void SubmitPrimitiveBounds(FD3D11LineBatchRenderer& InLineRenderer,
                                      const FPrimitiveRenderItem& InItem);
    static void SubmitSpriteBounds(FD3D11LineBatchRenderer& InLineRenderer,
                                   const FSceneView& InSceneView,
                                   const FSpriteRenderItem& InItem);
    static void SubmitTextBounds(FD3D11LineBatchRenderer& InLineRenderer,
                                 const FSceneView& InSceneView,
                                 const FTextRenderItem& InItem);
};
