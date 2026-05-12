#include "Editor/UI/EditorMainPanel.h"

#include "Editor/EditorEngine.h"

bool FEditorMainPanel::CanCloseEditor()
{
    return EditorEngine ? EditorEngine->GetSceneService().PromptSaveIfDirty() : true;
}

bool FEditorMainPanel::RequestNewScene()
{
    return EditorEngine && EditorEngine->GetSceneService().NewScene().bSuccess;
}

void FEditorMainPanel::RestoreLastSceneFromProjectSettings()
{
    if (!EditorEngine)
    {
        return;
    }

    EditorEngine->GetSceneService().RestoreLastScene();
}

bool FEditorMainPanel::RequestLoadSceneWithDialog()
{
    if (!EditorEngine || !EditorEngine->GetSceneService().PromptSaveIfDirty())
    {
        return false;
    }

    FString PickedPath;
    if (!Widgets.ToolbarWidget.OpenSceneFileDialog(PickedPath))
    {
        return false;
    }

    return EditorEngine->GetSceneService().OpenScene(PickedPath, false).bSuccess;
}

bool FEditorMainPanel::RequestSaveScene()
{
    return EditorEngine && EditorEngine->GetSceneService().SaveScene().bSuccess;
}

bool FEditorMainPanel::RequestSaveSceneAsWithDialog()
{
    FString PickedPath;
    if (!Widgets.ToolbarWidget.SaveSceneFileDialog(PickedPath))
    {
        return false;
    }

    return EditorEngine && EditorEngine->GetSceneService().SaveSceneToFilePath(PickedPath).bSuccess;
}
