#pragma once

#include "Renderer/Types/ViewMode.h"

class FViewportRenderSetting
{
public:
    // Viewport가 scene primitive를 어떤 방식으로 렌더링할지 결정하는 뷰 모드입니다.
    EViewModeIndex GetViewMode() const { return ViewMode; }
    void SetViewMode(EViewModeIndex InViewMode) { ViewMode = InViewMode; }

private:
    EViewModeIndex ViewMode = EViewModeIndex::Lit;
};
