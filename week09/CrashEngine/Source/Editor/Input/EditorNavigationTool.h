#pragma once

class FEditorViewportClient;
class FEditorViewportController;

class FEditorNavigationTool
{
public:
    FEditorNavigationTool(FEditorViewportClient* InOwner, FEditorViewportController* InController)
        : Owner(InOwner), Controller(InController)
    {
    }

    bool HandleInput(float DeltaTime);

private:
    bool HandleKeyboardAndMouseNavigation(float DeltaTime);
    bool HandleWheelZoom(float DeltaTime);

private:
    FEditorViewportClient* Owner = nullptr;
    FEditorViewportController* Controller = nullptr;
};
