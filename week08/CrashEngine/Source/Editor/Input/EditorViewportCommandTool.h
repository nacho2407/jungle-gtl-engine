#pragma once

#include "EditorViewportTool.h"

class FEditorViewportCommandTool : public FEditorViewportTool
{
public:
    using FEditorViewportTool::FEditorViewportTool;

    bool HandleInput(float DeltaTime) override;
};
