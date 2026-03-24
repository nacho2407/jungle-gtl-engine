#pragma once

#include "Core/Math/Color.h"
#include "Renderer/EditorRenderData.h"
#include "Renderer/Types/AxisColors.h"
#include "Renderer/Types/ViewMode.h"
#include "Renderer/Types/RenderItem.h"
#include "Renderer/Types/ObjectIdRenderItem.h"

class FD3D11MeshBatchRenderer;
class FD3D11ObjectIdRenderer;

struct FGizmoStyle
{
    float TranslationShaftLength = 20.0f;
    float TranslationShaftRadius = 10.f;
    float TranslationHeadLength = 5.0f;
    float TranslationHeadRadius = 10.f;

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
    void Submit(FD3D11ObjectIdRenderer& InObjectIdRenderer,
                const FEditorRenderData& InEditorRenderData) const;

  public:
    EViewModeIndex ViewMode = EViewModeIndex::VMI_Unlit;
    bool           bUseInstancing = true;
    FGizmoStyle    Style;

  private:
    FColor ResolveAxisColor(EAxis InAxis, EGizmoHighlight InHighlight) const;

    void AddTranslationGizmo(TArray<FPrimitiveRenderItem>& OutPrimitives,
                             const FGizmoDrawData&         InGizmoDrawData,
                             const FMatrix&                InGizmoMatrix) const;
    void AddRotationGizmo(TArray<FPrimitiveRenderItem>& OutPrimitives,
                          const FGizmoDrawData&         InGizmoDrawData,
                          const FMatrix&                InGizmoMatrix) const;
    void AddScalingGizmo(TArray<FPrimitiveRenderItem>& OutPrimitives,
                         const FGizmoDrawData&         InGizmoDrawData,
                         const FMatrix&                InGizmoMatrix) const;

    void AddTranslationGizmo(TArray<FObjectIdRenderItem>& OutItems, const FGizmoDrawData& InGizmoDrawData,
                             const FMatrix& InGizmoMatrix) const;
    void AddRotationGizmo(TArray<FObjectIdRenderItem>& OutItems, const FGizmoDrawData& InGizmoDrawData,
                          const FMatrix& InGizmoMatrix) const;
    void AddScalingGizmo(TArray<FObjectIdRenderItem>& OutItems, const FGizmoDrawData& InGizmoDrawData,
                         const FMatrix& InGizmoMatrix) const;
};