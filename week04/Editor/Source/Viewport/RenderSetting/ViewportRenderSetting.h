#pragma once

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

private:
    EViewModeIndex ViewMode = EViewModeIndex::VMI_Lit;
    bool           bShowGrid = true;
    bool           bShowSelectionOutline = true;
};
