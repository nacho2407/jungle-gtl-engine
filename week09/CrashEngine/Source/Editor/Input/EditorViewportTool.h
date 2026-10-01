#pragma once

#include "Core/RayTypes.h"

class FEditorViewportClient;
class FEditorViewportInputController;

class FEditorViewportTool
{
public:
    FEditorViewportTool(FEditorViewportClient* InOwner, FEditorViewportInputController* InController)
        : Owner(InOwner), Controller(InController)
    {
    }

    virtual ~FEditorViewportTool() = default;

    virtual bool HandleInput(float DeltaTime) = 0;
    virtual void ResetState() {}

protected:
    bool BuildMouseRay(FRay& OutRay) const;

    FEditorViewportClient* Owner = nullptr;
    FEditorViewportInputController* Controller = nullptr;
};
