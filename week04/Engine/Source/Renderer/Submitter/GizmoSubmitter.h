#pragma once

#include "Core/Math/Color.h"
#include "Renderer/EditorRenderData.h"
#include "Renderer/Types/AxisColors.h"
#include "Renderer/Types/ViewMode.h"
#include "Renderer/Types/RenderItem.h"

class FD3D11MeshBatchRenderer;

struct FGizmoStyle
{
    float TranslationShaftLength = 30.0f;
    float TranslationShaftRadius = 10.f;
    float TranslationHeadLength = 5.0f;
    float TranslationHeadRadius = 2.5f;

    float ScalingShaftLength = 30.0f;
    float ScalingShaftRadius = 10.0f;
    float ScalingHandleSize = 3.0f;

    float RotationRingRadius = 24.0f;
    float RotationRingThickness = 1.5f;
};

class FGizmoSubmitter
{
  public:
    void Submit(FD3D11MeshBatchRenderer& InMeshRenderer, const FEditorRenderData& InEditorRenderData);

  public:
    EViewModeIndex ViewMode = EViewModeIndex::VMI_Unlit;
    bool           bUseInstancing = true;
    FGizmoStyle    Style;

  private:
    FColor ResolveAxisColor(EAxis InAxis, EGizmoHighlight InHighlight) const;

    void AddTranslationGizmo(TArray<FPrimitiveRenderItem>& OutPrimitives,
                             const FGizmoDrawData&         InGizmoDrawData) const;
    void AddRotationGizmo(TArray<FPrimitiveRenderItem>& OutPrimitives,
                          const FGizmoDrawData&         InGizmoDrawData) const;
    void AddScalingGizmo(TArray<FPrimitiveRenderItem>& OutPrimitives,
                         const FGizmoDrawData&         InGizmoDrawData) const;
};