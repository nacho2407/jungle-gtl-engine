#pragma once

#include "Renderer/Types/EditorShowFlags.h"
#include "Renderer/Types/SceneShowFlags.h"
#include "Renderer/Types/ViewMode.h"

class FViewportRenderSetting
{
public:
    // Viewport가 scene primitive를 어떤 방식으로 렌더링할지 결정하는 뷰 모드입니다.
    EViewModeIndex GetViewMode() const { return ViewMode; }
    void SetViewMode(EViewModeIndex InViewMode) { ViewMode = InViewMode; }

    bool IsGridVisible() const { return bShowGrid; }
    void SetGridVisible(bool bInShowGrid) { bShowGrid = bInShowGrid; }

    bool IsSelectionOutlineVisible() const { return bShowSelectionOutline; }
    void SetSelectionOutlineVisible(bool bInShowSelectionOutline)
    {
        bShowSelectionOutline = bInShowSelectionOutline;
    }

    bool IsSelectionOutlineOcclusionEnabled() const { return bEnableSelectionOutlineOcclusion; }
    void SetSelectionOutlineOcclusionEnabled(bool bInEnableSelectionOutlineOcclusion)
    {
        bEnableSelectionOutlineOcclusion = bInEnableSelectionOutlineOcclusion;
    }

    bool IsWorldAxesVisible() const { return bShowWorldAxes; }
    void SetWorldAxesVisible(bool bInShowWorldAxes) { bShowWorldAxes = bInShowWorldAxes; }

    bool IsGizmoVisible() const { return bShowGizmo; }
    void SetGizmoVisible(bool bInShowGizmo) { bShowGizmo = bInShowGizmo; }

    bool IsObjectLabelsVisible() const { return bShowObjectLabels; }
    void SetObjectLabelsVisible(bool bInShowObjectLabels)
    {
        bShowObjectLabels = bInShowObjectLabels;
    }

    EEditorShowFlags BuildEditorShowFlags(bool bIncludeGizmo) const
    {
        EEditorShowFlags Flags = EEditorShowFlags::None;
        if (bShowGrid)
        {
            Flags |= EEditorShowFlags::SF_Grid;
        }
        if (bShowWorldAxes)
        {
            Flags |= EEditorShowFlags::SF_WorldAxes;
        }
        if (bShowSelectionOutline)
        {
            Flags |= EEditorShowFlags::SF_SelectionOutline;
        }
        if (bShowObjectLabels)
        {
            Flags |= EEditorShowFlags::SF_ObjectLabels;
        }
        if (bIncludeGizmo && bShowGizmo)
        {
            Flags |= EEditorShowFlags::SF_Gizmo;
        }
        return Flags;
    }

private:
    EViewModeIndex ViewMode = EViewModeIndex::VMI_Lit;
    bool           bShowGrid = true;
    bool           bShowWorldAxes = true;
    bool           bShowGizmo = true;
    bool           bShowSelectionOutline = true;
    bool           bEnableSelectionOutlineOcclusion = true;
    bool           bShowObjectLabels = true;
    bool           bShowUUIDLabels = true;
    bool           bShowScenePrimitives = true;
    bool           bShowSceneSprites = true;
    bool           bShowBillboardText = true;
};
