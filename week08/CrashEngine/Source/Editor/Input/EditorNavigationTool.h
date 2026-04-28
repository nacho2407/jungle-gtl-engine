#pragma once

#include "EditorViewportTool.h"

class FEditorNavigationTool : public FEditorViewportTool
{
public:
    using FEditorViewportTool::FEditorViewportTool;

    bool HandleInput(float DeltaTime) override;

private:
    bool HandleKeyboardAndMouseNavigation(float DeltaTime);
    bool HandleWheelZoom(float DeltaTime);
};
