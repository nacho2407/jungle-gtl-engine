#pragma once
#pragma once

#include "Engine/ViewPort/ViewportController.h"

class FScene;
struct FEditorContext;
class FViewportSelectionController;

class FEditorGlobalController : public Engine::Viewport::IViewportController
{
public:
    FEditorGlobalController() = default;
    ~FEditorGlobalController() override = default;

    void SetScene(FScene* InScene) { Scene = InScene; }
    void SetEditorContext(FEditorContext* InContext) { Context = InContext; }
    FEditorContext* GetEditorContext() const { return Context; }
    void SetSelectionController(FViewportSelectionController* InSelectionController)
    {
        SelectionController = InSelectionController;
    }

    bool CanDeleteSelectedActors() const;
    bool DeleteSelectedActors();

    void NewScene();
    void OpenScene();
    void SaveScene();
    void SaveSceneAs();
    void RequestAboutPopup();
    void RequestConsoleInputFocus();

private:
    FScene* Scene = nullptr;
    FEditorContext* Context = nullptr;
    FViewportSelectionController* SelectionController = nullptr;
};
