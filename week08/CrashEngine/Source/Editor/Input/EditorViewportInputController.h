#pragma once

#include <memory>

class FEditorViewportClient;
class FEditorViewportCommandTool;
class FEditorGizmoTool;
class FEditorSelectionTool;
class FEditorNavigationTool;

class FEditorViewportController
{
public:
    explicit FEditorViewportController(FEditorViewportClient* InOwner);

    bool HandleViewportCommandInput(float DeltaTime);
    bool HandleGizmoInput(float DeltaTime);
    bool HandleSelectionInput(float DeltaTime);
    bool HandleNavigationInput(float DeltaTime);

private:
    FEditorViewportClient* Owner = nullptr;

    std::unique_ptr<FEditorViewportCommandTool> CommandTool;
    std::unique_ptr<FEditorGizmoTool> GizmoTool;
    std::unique_ptr<FEditorSelectionTool> SelectionTool;
    std::unique_ptr<FEditorNavigationTool> NavigationTool;
};
