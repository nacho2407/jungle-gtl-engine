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
    float TranslationShaftLength = 1.0f;
    float TranslationShaftRadius = 1.5f;
    float TranslationHeadLength = 0.5f;
    float TranslationHeadRadius = 1.5f;

    float ScalingShaftLength = 1.0f;
    float ScalingShaftRadius = 1.5f;
    float ScalingHandleSize = 0.3f;

    float RotationRingRadius = 1.0f;
    float RotationRingThickness = 0.05f;

    float CenterHandleRadius = 0.24f;
};

class FOverlayMeshSubmitter
{
  public:
    void Submit(FD3D11MeshBatchRenderer& InMeshRenderer,
                const FEditorRenderData&   InEditorRenderData) const;
    void SubmitCenterHandle(FD3D11MeshBatchRenderer& InMeshRenderer,
                            const FEditorRenderData&   InEditorRenderData) const;
    void Submit(FD3D11ObjectIdRenderer&  InObjectIdRenderer,
                const FEditorRenderData& InEditorRenderData) const;

  public:
    FGizmoStyle Style;

  private:
    static constexpr EAxis GizmoAxisOrder[] = {EAxis::X, EAxis::Y, EAxis::Z};

    FColor ResolveAxisColor(EAxis InAxis, EGizmoHighlight InHighlight) const;

    void AddCenterHandle(TArray<FPrimitiveRenderItem>& OutPrimitives,
                         const FGizmoDrawData& InGizmoDrawData, const FMatrix& InGizmoMatrix) const;
    void AddTranslationGizmo(TArray<FPrimitiveRenderItem>& OutPrimitives,
                             const FGizmoDrawData&         InGizmoDrawData,
                             const FMatrix&                InGizmoMatrix) const;
    void AddRotationGizmo(TArray<FPrimitiveRenderItem>& OutPrimitives,
                          const FGizmoDrawData&         InGizmoDrawData,
                          const FMatrix&                InGizmoMatrix) const;
    void AddScalingGizmo(TArray<FPrimitiveRenderItem>& OutPrimitives,
                         const FGizmoDrawData& InGizmoDrawData, const FMatrix& InGizmoMatrix) const;

    void AddCenterHandle(TArray<FObjectIdRenderItem>& OutItems,
                         const FGizmoDrawData& InGizmoDrawData, const FMatrix& InGizmoMatrix) const;
    void AddTranslationGizmo(TArray<FObjectIdRenderItem>& OutItems,
                             const FGizmoDrawData&        InGizmoDrawData,
                             const FMatrix&               InGizmoMatrix) const;
    void AddRotationGizmo(TArray<FObjectIdRenderItem>& OutItems,
                          const FGizmoDrawData&        InGizmoDrawData,
                          const FMatrix&               InGizmoMatrix) const;
    void AddScalingGizmo(TArray<FObjectIdRenderItem>& OutItems,
                         const FGizmoDrawData& InGizmoDrawData, const FMatrix& InGizmoMatrix) const;
};