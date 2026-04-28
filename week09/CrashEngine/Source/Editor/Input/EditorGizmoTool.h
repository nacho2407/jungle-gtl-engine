#pragma once

#include "Core/RayTypes.h"

class FEditorViewportClient;
class FEditorViewportController;

class FEditorGizmoTool
{
public:
    FEditorGizmoTool(FEditorViewportClient* InOwner, FEditorViewportController* InController)
        : Owner(InOwner), Controller(InController)
    {
    }

    bool HandleInput(float DeltaTime);

private:
    bool BuildMouseRay(FRay& OutRay) const;
    bool HandleDragStart(const FRay& Ray);
    bool HandleDragUpdate(const FRay& Ray);
    bool HandleDragEnd();

private:
    FEditorViewportClient* Owner = nullptr;
    FEditorViewportController* Controller = nullptr;
};
