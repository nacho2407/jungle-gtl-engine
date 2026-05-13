#include "Editor/UI/EditorMainPanel.h"

#include "Editor/EditorEngine.h"

bool FEditorMainPanel::CanCloseEditor()
{
    return EditorEngine ? EditorEngine->GetSceneService().PromptSaveIfDirty() : true;
}

bool FEditorMainPanel::RequestNewScene()
{
    return EditorEngine && EditorEngine->GetCommandSystem().Execute(EEditorCommand::NewScene);
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

    return EditorEngine->GetCommandSystem().Execute(EEditorCommand::OpenScene, { PickedPath });
}

bool FEditorMainPanel::RequestSaveScene()
{
    return EditorEngine && EditorEngine->GetCommandSystem().Execute(EEditorCommand::SaveScene);
}

bool FEditorMainPanel::RequestSaveSceneAsWithDialog()
{
    FString PickedPath;
    if (!Widgets.ToolbarWidget.SaveSceneFileDialog(PickedPath))
    {
        return false;
    }

    return EditorEngine && EditorEngine->GetCommandSystem().Execute(EEditorCommand::SaveSceneAs, { PickedPath });
}
