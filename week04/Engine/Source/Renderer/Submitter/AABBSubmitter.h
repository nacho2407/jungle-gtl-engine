#pragma once

#include "Renderer/SceneRenderData.h"

class FD3D11LineBatchRenderer;
class FSceneView;
struct FPrimitiveRenderItem;
struct FStaticMeshRenderItem;

class FAABBSubmitter
{
  public:
    void Submit(FD3D11LineBatchRenderer& InLineRenderer,
                const FSceneRenderData&  InSceneRenderData) const;

  private:
    inline static const FColor SelectedBoundsColor = FColor(0.1f, 0.4f, 1.0f, 1.0f); // Blue
    inline static const FColor HoveredBoundsColor = FColor(1.0f, 0.9f, 0.1f, 1.0f);  // Yellow
    inline static const FColor DefaultBoundsColor = FColor(1.0f, 1.0f, 1.0f, 1.0f);  // White

    static FColor ResolveBoundsColor(const FRenderItemState& InState);
    static void   ExpandBounds(FVector& InOutMin, FVector& InOutMax, const FVector& InPoint);
    static void   SubmitBox(FD3D11LineBatchRenderer& InLineRenderer, const FVector& InMin,
                            const FVector& InMax, const FColor& InColor);
    static void   SubmitWorldAABB(FD3D11LineBatchRenderer& InLineRenderer,
                                  const Geometry::FAABB&   InWorldAABB,
                                  const FRenderItemState&  InState);

    static void SubmitPrimitiveBounds(FD3D11LineBatchRenderer&    InLineRenderer,
                                      const FPrimitiveRenderItem& InItem);
    static void SubmitStaticMeshBounds(FD3D11LineBatchRenderer&     InLineRenderer,
                                       const FStaticMeshRenderItem& InItem);
};