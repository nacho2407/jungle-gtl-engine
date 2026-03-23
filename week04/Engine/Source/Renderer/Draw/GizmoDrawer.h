#pragma once

#include "Core/Math/Color.h"
#include "Renderer/EditorRenderData.h"
#include "Renderer/SceneRenderData.h"
#include "Renderer/Types/AxisColors.h"
#include "Renderer/Types/ViewMode.h"

class FD3D11MeshBatchRenderer;

class FGizmoDrawer
{
  public:
    void Draw(FD3D11MeshBatchRenderer& InMeshRenderer, const FEditorRenderData& InEditorRenderData);

  public:
    EViewModeIndex ViewMode = EViewModeIndex::Unlit;
    bool           bUseInstancing = true;

  private:
    FColor ResolveAxisColor(EAxis InAxis, EGizmoHighlight InHighlight) const;
    void   AddTranslationGizmo(TArray<FPrimitiveRenderItem>& OutPrimitives,
                               const FGizmoDrawData&         InGizmoDrawData) const;
    void   AddRotationGizmo(TArray<FPrimitiveRenderItem>& OutPrimitives,
                            const FGizmoDrawData&         InGizmoDrawData) const;
    void   AddScalingGizmo(TArray<FPrimitiveRenderItem>& OutPrimitives,
                           const FGizmoDrawData&         InGizmoDrawData) const;
};
