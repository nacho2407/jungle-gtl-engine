#pragma once

#include "Core/RayTypes.h"
#include "EditorViewportTool.h"

class FEditorSelectionTool : public FEditorViewportTool
{
public:
    using FEditorViewportTool::FEditorViewportTool;

    bool HandleInput(float DeltaTime) override;

private:
    bool HandleWorldPicking(const FRay& Ray);
};
