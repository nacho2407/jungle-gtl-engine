#include "Editor/UI/EditorToolbarWidget.h"

#include "Editor/EditorEngine.h"
#include "Editor/UI/EditorViewportOverlayWidget.h"
#include "Editor/UI/EditorPlayStreamWidget.h"
#include "Core/Paths.h"
#include "Core/ResourceManager.h"
#include "Serialization/SceneSaveManager.h"
#include "ImGui/imgui.h"

#include <Windows.h>
#include <commdlg.h>
#include <filesystem>
#include <shellapi.h>
#include <utility>

namespace
{
	std::wstring GetSceneDialogInitialDir()
	{
		std::filesystem::path SceneDir(FSceneSaveManager::GetSceneDirectory());
		SceneDir = SceneDir.lexically_normal();
		if (!SceneDir.is_absolute())
		{
			SceneDir = std::filesystem::path(FPaths::ToAbsolute(SceneDir.wstring()));
		}
		SceneDir.make_preferred();
		std::error_code Ec;
		std::filesystem::create_directories(SceneDir, Ec);
		return SceneDir.wstring();
	}
}

bool FEditorToolbarWidget::OpenSceneFileDialog(FString& OutFilePath) const
{
	OutFilePath.clear();

	WCHAR FileBuffer[MAX_PATH] = { 0 };
	const std::wstring InitialDir = GetSceneDialogInitialDir();
	const std::filesystem::path PrevCwd = std::filesystem::current_path();
	std::error_code ChdirEc;
	std::filesystem::current_path(std::filesystem::path(InitialDir), ChdirEc);

	const std::wstring OpenPattern = std::filesystem::path(InitialDir).append(L"*.Scene").wstring();
	wcsncpy_s(FileBuffer, MAX_PATH, OpenPattern.c_str(), _TRUNCATE);

	OPENFILENAMEW DialogDesc = {};
	DialogDesc.lStructSize = sizeof(DialogDesc);
	DialogDesc.hwndOwner = static_cast<HWND>(ImGui::GetMainViewport()->PlatformHandleRaw);
	DialogDesc.lpstrFilter = L"Scene Files (*.Scene)\0*.Scene\0All Files (*.*)\0*.*\0";
	DialogDesc.lpstrFile = FileBuffer;
	DialogDesc.nMaxFile = MAX_PATH;
	DialogDesc.lpstrInitialDir = InitialDir.c_str();
	DialogDesc.lpstrDefExt = L"Scene";
	DialogDesc.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

	const BOOL bPicked = GetOpenFileNameW(&DialogDesc);
	std::error_code RestoreEc;
	std::filesystem::current_path(PrevCwd, RestoreEc);
	if (!bPicked)
	{
		return false;
	}

	OutFilePath = FPaths::ToUtf8(FileBuffer);
	return true;
}

bool FEditorToolbarWidget::SaveSceneFileDialog(FString& OutFilePath) const
{
	OutFilePath.clear();

	WCHAR FileBuffer[MAX_PATH] = { 0 };
	const std::wstring InitialDir = GetSceneDialogInitialDir();
	const std::filesystem::path PrevCwd = std::filesystem::current_path();
	std::error_code ChdirEc;
	std::filesystem::current_path(std::filesystem::path(InitialDir), ChdirEc);

	const std::wstring DefaultFile = std::filesystem::path(InitialDir).append(L"NewScene.Scene").wstring();
	wcsncpy_s(FileBuffer, MAX_PATH, DefaultFile.c_str(), _TRUNCATE);

	OPENFILENAMEW DialogDesc = {};
	DialogDesc.lStructSize = sizeof(DialogDesc);
	DialogDesc.hwndOwner = static_cast<HWND>(ImGui::GetMainViewport()->PlatformHandleRaw);
	DialogDesc.lpstrFilter = L"Scene Files (*.Scene)\0*.Scene\0All Files (*.*)\0*.*\0";
	DialogDesc.lpstrFile = FileBuffer;
	DialogDesc.nMaxFile = MAX_PATH;
	DialogDesc.lpstrInitialDir = InitialDir.c_str();
	DialogDesc.lpstrDefExt = L"Scene";
	DialogDesc.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

	const BOOL bPicked = GetSaveFileNameW(&DialogDesc);
	std::error_code RestoreEc;
	std::filesystem::current_path(PrevCwd, RestoreEc);
	if (!bPicked)
	{
		return false;
	}

	OutFilePath = FPaths::ToUtf8(FileBuffer);
	return true;
}

void FEditorToolbarWidget::SetViewportOverlayWidget(FEditorViewportOverlayWidget* InViewportOverlayWidget)
{
	ViewportOverlayWidget = InViewportOverlayWidget;
}

void FEditorToolbarWidget::SetPlayStreamWidget(FEditorPlayStreamWidget* InPlayStreamWidget)
{
	PlayStreamWidget = InPlayStreamWidget;
}

void FEditorToolbarWidget::SetPIEViewportFullscreenCallback(std::function<void(bool)> InCallback)
{
	PIEViewportFullscreenCallback = std::move(InCallback);
}

void FEditorToolbarWidget::SetBuildGameCallback(std::function<void()> InCallback)
{
	BuildGameCallback = std::move(InCallback);
}

void FEditorToolbarWidget::SetPanelVisibilityRefs(
	bool* InShowConsole,
	bool* InShowControl,
	bool* InShowProperty,
	bool* InShowSceneManager,
	bool* InShowMaterialEditor,
	bool* InShowStatProfiler,
	bool* InShowEditorDebug,
	bool* InShowContentBrowser,
	bool* InShowUndoHistory,
	bool* InShowRuntimeUIPreview,
	bool* InShowProjectSettings,
	bool* InShowWorldSettings,
	bool* InPIEViewportFullscreenEnabled)
{
	bShowConsole = InShowConsole;
	bShowControl = InShowControl;
	bShowProperty = InShowProperty;
	bShowSceneManager = InShowSceneManager;
	bShowMaterialEditor = InShowMaterialEditor;
	bShowStatProfiler = InShowStatProfiler;
	bShowEditorDebug = InShowEditorDebug;
	bShowContentBrowser = InShowContentBrowser;
	bShowUndoHistory = InShowUndoHistory;
	bShowRuntimeUIPreview = InShowRuntimeUIPreview;
	bShowProjectSettings = InShowProjectSettings;
	bShowWorldSettings = InShowWorldSettings;
	bPIEViewportFullscreenEnabled = InPIEViewportFullscreenEnabled;
}

void FEditorToolbarWidget::Render(float DeltaTime)
{
	(void)DeltaTime;

	const ImGuiIO& IO = ImGui::GetIO();
	if (EditorEngine && !IO.WantTextInput && IO.KeyCtrl)
	{
		if (ImGui::IsKeyPressed(ImGuiKey_N, false))
		{
			EditorEngine->GetCommandSystem().Execute(EEditorCommand::NewScene);
		}
		if (ImGui::IsKeyPressed(ImGuiKey_O, false))
		{
			if (EditorEngine->GetSceneService().PromptSaveIfDirty())
			{
				FString PickedPath;
				if (OpenSceneFileDialog(PickedPath))
				{
					EditorEngine->GetCommandSystem().Execute(EEditorCommand::OpenScene, { PickedPath });
				}
			}
		}
		if (ImGui::IsKeyPressed(ImGuiKey_S, false))
		{
			if (IO.KeyShift)
			{
				FString PickedPath;
				if (SaveSceneFileDialog(PickedPath))
				{
					EditorEngine->GetCommandSystem().Execute(EEditorCommand::SaveSceneAs, { PickedPath });
				}
			}
			else
			{
				EditorEngine->GetCommandSystem().Execute(EEditorCommand::SaveScene);
			}
		}
	}

	ImVec2 OriginalPadding = ImGui::GetStyle().FramePadding;
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(OriginalPadding.x, 5.0f));

	bool bMenuBarOpened = ImGui::BeginMainMenuBar();

	ImGui::PopStyleVar();

	if (!bMenuBarOpened)
	{
		return;
	}

	RenderFilesMenu();
	RenderEditMenu();
	RenderBuildMenu();
	RenderWindowMenu();
	RenderSettingsMenu();
	RenderHelpMenu();

	ImGui::EndMainMenuBar();
}

void FEditorToolbarWidget::RenderFilesMenu()
{
	if (!ImGui::BeginMenu("File"))
	{
		return;
	}

	if (EditorEngine)
	{
		if (ImGui::MenuItem("New Scene", "Ctrl+N"))
		{
			EditorEngine->GetCommandSystem().Execute(EEditorCommand::NewScene);
		}
		if (ImGui::MenuItem("Load Scene", "Ctrl+O"))
		{
			if (EditorEngine->GetSceneService().PromptSaveIfDirty())
			{
				FString PickedPath;
				if (OpenSceneFileDialog(PickedPath))
				{
					EditorEngine->GetCommandSystem().Execute(EEditorCommand::OpenScene, { PickedPath });
				}
			}
		}
		if (ImGui::MenuItem("Save Scene", "Ctrl+S"))
		{
			EditorEngine->GetCommandSystem().Execute(EEditorCommand::SaveScene);
		}
		if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S"))
		{
			FString PickedPath;
			if (SaveSceneFileDialog(PickedPath))
			{
				EditorEngine->GetCommandSystem().Execute(EEditorCommand::SaveSceneAs, { PickedPath });
			}
		}

		ImGui::Separator();

		if (ImGui::MenuItem("Reload Asset From Disk"))
		{
			EditorEngine->GetCommandSystem().Execute(EEditorCommand::RefreshAssets);
		}
		if (ImGui::MenuItem("Open Asset Folder"))
		{
			const std::wstring AssetDir = FPaths::ToAbsolute(L"Asset");
			ShellExecuteW(nullptr, L"open", AssetDir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
		}
	}
	else
	{
		ImGui::MenuItem("New Scene", "Ctrl+N", false, false);
		ImGui::MenuItem("Load Scene", "Ctrl+O", false, false);
		ImGui::MenuItem("Save Scene", "Ctrl+S", false, false);
		ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S", false, false);
		ImGui::Separator();
		ImGui::MenuItem("Reload Asset From Disk", nullptr, false, false);
		ImGui::MenuItem("Open Asset Folder", nullptr, false, false);
	}

	ImGui::EndMenu();
}

void FEditorToolbarWidget::RenderBuildMenu()
{
	if (!ImGui::BeginMenu("Build"))
	{
		return;
	}

	if (ImGui::MenuItem("Packaging...", nullptr, false, BuildGameCallback != nullptr))
	{
		if (BuildGameCallback)
		{
			BuildGameCallback();
		}
	}

	ImGui::EndMenu();
}

void FEditorToolbarWidget::RenderEditMenu()
{
	if (!ImGui::BeginMenu("Edit"))
	{
		return;
	}

	const bool bCanUndo = EditorEngine && EditorEngine->GetCommandSystem().CanExecute(EEditorCommand::Undo);
	const bool bCanRedo = EditorEngine && EditorEngine->GetCommandSystem().CanExecute(EEditorCommand::Redo);
	if (ImGui::MenuItem("Undo", "Ctrl+Z", false, bCanUndo) && EditorEngine)
	{
		EditorEngine->GetCommandSystem().Execute(EEditorCommand::Undo);
	}
	if (ImGui::MenuItem("Redo", "Ctrl+Shift+Z", false, bCanRedo) && EditorEngine)
	{
		EditorEngine->GetCommandSystem().Execute(EEditorCommand::Redo);
	}

	ImGui::Separator();
	if (bShowUndoHistory)
	{
		ImGui::MenuItem("Undo History", nullptr, bShowUndoHistory);
	}
	else
	{
		ImGui::MenuItem("Undo History", nullptr, false, false);
	}

	ImGui::EndMenu();
}

void FEditorToolbarWidget::RenderWindowMenu()
{
	if (!ImGui::BeginMenu("Window"))
	{
		return;
	}

	if (bShowConsole) ImGui::MenuItem("Console Drawer", nullptr, bShowConsole);
	if (bShowControl) ImGui::MenuItem("Jungle Control Panel", nullptr, bShowControl);
	if (bShowProperty) ImGui::MenuItem("Details", nullptr, bShowProperty);
	if (bShowSceneManager) ImGui::MenuItem("Outliner", nullptr, bShowSceneManager);
	if (bShowMaterialEditor) ImGui::MenuItem("Material Editor", nullptr, bShowMaterialEditor);
	if (bShowStatProfiler) ImGui::MenuItem("Stat Profiler", nullptr, bShowStatProfiler);
	if (bShowContentBrowser) ImGui::MenuItem("Content Browser", "Ctrl+Space", bShowContentBrowser);
	if (bShowRuntimeUIPreview) ImGui::MenuItem("Runtime UI Preview", nullptr, bShowRuntimeUIPreview);
	if (bShowProjectSettings) ImGui::MenuItem("Project Settings", nullptr, bShowProjectSettings);
	if (bShowWorldSettings) ImGui::MenuItem("World Settings", nullptr, bShowWorldSettings);

	ImGui::EndMenu();
}

void FEditorToolbarWidget::RenderSettingsMenu()
{
	if (!ImGui::BeginMenu("Settings"))
	{
		return;
	}

	if (bShowEditorDebug)
	{
		ImGui::MenuItem("Editor Debug", nullptr, bShowEditorDebug);
	}

	if (ViewportOverlayWidget)
	{
		bool bShowViewportSettings = ViewportOverlayWidget->IsViewportSettingsVisible();
		if (ImGui::MenuItem("Viewport Settings", nullptr, bShowViewportSettings))
		{
			ViewportOverlayWidget->SetViewportSettingsVisible(!bShowViewportSettings);
		}
		bool bShowGroupedStats = ViewportOverlayWidget->IsGroupedStatOverlayVisible();
		if (ImGui::MenuItem("Grouped Stat Overlay", nullptr, bShowGroupedStats))
		{
			ViewportOverlayWidget->SetGroupedStatOverlayVisible(!bShowGroupedStats);
		}
	}

	if (bPIEViewportFullscreenEnabled)
	{
		const bool bEnabled = *bPIEViewportFullscreenEnabled;
		if (ImGui::MenuItem("PIE Fullscreen Viewport", nullptr, bEnabled))
		{
			if (PIEViewportFullscreenCallback)
			{
				PIEViewportFullscreenCallback(!bEnabled);
			}
			else
			{
				*bPIEViewportFullscreenEnabled = !bEnabled;
			}
		}
	}

	ImGui::Separator();

	if (ImGui::MenuItem("Remove Cache from Disk"))
	{
		FResourceManager::Get().DeleteAllCacheFiles();
	}

	ImGui::EndMenu();
}

void FEditorToolbarWidget::RenderHelpMenu()
{
	if (!ImGui::BeginMenu("Help"))
	{
		return;
	}

	if (ViewportOverlayWidget)
	{
		if (ImGui::MenuItem("Shortcuts"))
		{
			ViewportOverlayWidget->SetShortcutsWindowVisible(true);
		}
	}
	else
	{
		ImGui::MenuItem("Shortcuts", nullptr, false, false);
	}

	ImGui::EndMenu();
}
