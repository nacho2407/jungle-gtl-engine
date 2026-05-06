#include "Editor/UI/EditorMainPanel.h"

#include "Editor/Settings/ProjectSettings.h"
#include "Engine/Core/Paths.h"
#include "Core/Logging/Log.h"

#include <filesystem>

bool FEditorMainPanel::CanCloseEditor()
{
    return Widgets.SceneWidget.PromptSaveIfDirty();
}

bool FEditorMainPanel::RequestNewScene()
{
    Widgets.SceneWidget.NewScene();
    return true;
}

void FEditorMainPanel::RestoreLastSceneFromProjectSettings()
{
    if (!EditorEngine)
    {
        return;
    }

    FProjectSettings& ProjectSettings = FProjectSettings::Get();
    if (ProjectSettings.HasSavedLastScenePath())
    {
        const FString StoredScenePath = ProjectSettings.LastScenePath;
        const std::filesystem::path ScenePath(FPaths::ToAbsolute(FPaths::ToWide(StoredScenePath)));

        std::error_code Ec;
        const bool bSceneExists = std::filesystem::exists(ScenePath, Ec) && !Ec;
        Ec.clear();
        const bool bSceneFile = bSceneExists && std::filesystem::is_regular_file(ScenePath, Ec) && !Ec;
        if (bSceneFile)
        {
            if (Widgets.SceneWidget.LoadSceneFromFilePath(FPaths::ToUtf8(ScenePath.wstring()), false))
            {
                return;
            }

            UE_LOG_WARNING("[ProjectSettings] Failed to load last scene: %s", StoredScenePath.c_str());
        }
        else
        {
            UE_LOG_WARNING(
                "[ProjectSettings] Last scene path is invalid, opening New Scene: %s",
                StoredScenePath.c_str()
            );
        }
    }

    Widgets.SceneWidget.NewScene();
}

bool FEditorMainPanel::RequestLoadSceneWithDialog()
{
    if (!Widgets.SceneWidget.PromptSaveIfDirty())
    {
        return false;
    }

    FString PickedPath;
    if (!Widgets.ToolbarWidget.OpenSceneFileDialog(PickedPath))
    {
        return false;
    }

    return Widgets.SceneWidget.LoadSceneFromFilePath(PickedPath, false);
}

bool FEditorMainPanel::RequestSaveScene()
{
    return Widgets.SceneWidget.SaveScene();
}

bool FEditorMainPanel::RequestSaveSceneAsWithDialog()
{
    FString PickedPath;
    if (!Widgets.ToolbarWidget.SaveSceneFileDialog(PickedPath))
    {
        return false;
    }

    return Widgets.SceneWidget.SaveSceneToFilePath(PickedPath);
}
