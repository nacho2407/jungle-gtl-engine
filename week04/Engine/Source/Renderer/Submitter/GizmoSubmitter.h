#pragma once

#include "Core/Math/Color.h"
#include "Renderer/EditorRenderData.h"
#include "Renderer/Types/AxisColors.h"
#include "Renderer/Types/ViewMode.h"
#include "Renderer/Types/RenderItem.h"
#include "Renderer/Types/ObjectIdRenderItem.h"

class FD3D11OverlayMeshRenderer;
class FD3D11ObjectIdRenderer;

struct FGizmoStyle
{
    float TranslationShaftLength = 1.0f;
    float TranslationShaftRadius = 1.5f;
    float TranslationHeadLength = 0.5f;
    float TranslationHeadRadius = 1.5f;

    float ScalingShaftLength = 1.0f;
    float ScalingShaftRadius = 1.5f;
    float ScalingHandleSize = 0.3f;

    float RotationRingRadius = 1.0f;
    float RotationRingThickness = 0.05f;
};

class FGizmoSubmitter
{
  public:
    void Submit(FD3D11OverlayMeshRenderer& InMeshRenderer,
                const FEditorRenderData&   InEditorRenderData);
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