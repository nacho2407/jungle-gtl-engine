#pragma once

#include "EditorViewportTool.h"
#include "Input/InputTypes.h"

class FEditorViewportContextMenuTool : public FEditorViewportTool
{
public:
    using FEditorViewportTool::FEditorViewportTool;

    bool HandleInput(float DeltaTime) override;

private:
    bool HandleContextMenuRequest();

    bool bRightClickCandidate = false;
    POINT RightClickStartLocalPos = { 0, 0 };
};
