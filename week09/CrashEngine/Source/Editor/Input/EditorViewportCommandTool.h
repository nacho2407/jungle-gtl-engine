#pragma once

class FEditorViewportClient;
class FEditorViewportController;

class FEditorViewportCommandTool
{
public:
    FEditorViewportCommandTool(FEditorViewportClient* InOwner, FEditorViewportController* InController)
        : Owner(InOwner), Controller(InController)
    {
    }

    bool HandleInput(float DeltaTime);

private:
    FEditorViewportClient* Owner = nullptr;
    FEditorViewportController* Controller = nullptr;
};
