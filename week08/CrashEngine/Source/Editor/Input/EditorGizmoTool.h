#pragma once

#include "Core/RayTypes.h"
#include "EditorViewportTool.h"

class FEditorGizmoTool : public FEditorViewportTool
{
public:
    using FEditorViewportTool::FEditorViewportTool;

    bool HandleInput(float DeltaTime) override;
    void ResetState() override;

private:
    bool HandleDragStart(const FRay& Ray);
    bool HandleDragUpdate(const FRay& Ray);
    bool HandleDragEnd();
};
