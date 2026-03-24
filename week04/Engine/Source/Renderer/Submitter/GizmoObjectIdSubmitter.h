#pragma once

#include "Core/Containers/Array.h"
#include "Renderer/EditorRenderData.h"
#include "Renderer/Submitter/GizmoSubmitter.h"
#include "Renderer/Types/ObjectIdRenderItem.h"
#include "Renderer/Types/AxisColors.h"

class FGizmoObjectIdSubmitter
{
  public:
    void Submit(TArray<FObjectIdRenderItem>& OutItems, const FEditorRenderData& InEditorRenderData) const;

    FGizmoStyle Style;

  private:
    void AddTranslationGizmo(TArray<FObjectIdRenderItem>& OutItems,
                             const FGizmoDrawData&       InGizmoDrawData) const;
    void AddRotationGizmo(TArray<FObjectIdRenderItem>& OutItems,
                          const FGizmoDrawData&       InGizmoDrawData) const;
    void AddScalingGizmo(TArray<FObjectIdRenderItem>& OutItems,
                         const FGizmoDrawData&       InGizmoDrawData) const;
};
