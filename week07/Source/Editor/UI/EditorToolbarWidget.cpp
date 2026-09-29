#include "Editor/UI/EditorToolbarWidget.h"

#include "Editor/UI/EditorSceneWidget.h"
#include "Editor/UI/EditorViewportOverlayWidget.h"
#include "Editor/UI/EditorPlayStreamWidget.h"
#include "Core/ResourceManager.h"
#include "Serialization/SceneSaveManager.h"
#include "ImGui/imgui.h"

#include <Windows.h>
#include <commdlg.h>
#include <filesystem>
#include <shellapi.h>

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

void FEditorToolbarWidget::SetSceneWidget(FEditorSceneWidget* InSceneWidget)
{
	SceneWidget = InSceneWidget;
}

void FEditorToolbarWidget::SetPlayStreamWidget(FEditorPlayStreamWidget* InPlayStreamWidget)
{
	PlayStreamWidget = InPlayStreamWidget;
}

void FEditorToolbarWidget::SetPanelVisibilityRefs(
	bool* InShowConsole,
	bool* InShowControl,
	bool* InShowProperty,
	bool* InShowSceneManager,
	bool* InShowMaterialEditor,
	bool* InShowStatProfiler)
{
	bShowConsole = InShowConsole;
	bShowControl = InShowControl;
	bShowProperty = InShowProperty;
	bShowSceneManager = InShowSceneManager;
	bShowMaterialEditor = InShowMaterialEditor;
	bShowStatProfiler = InShowStatProfiler;
}

void FEditorToolbarWidget::Render(float DeltaTime)
{
	(void)DeltaTime;

	const ImGuiIO& IO = ImGui::GetIO();
	if (SceneWidget && !IO.WantTextInput && IO.KeyCtrl)
	{
		if (ImGui::IsKeyPressed(ImGuiKey_N, false))
		{
			SceneWidget->NewScene();
		}
		if (ImGui::IsKeyPressed(ImGuiKey_O, false))
		{
			FString PickedPath;
			if (OpenSceneFileDialog(PickedPath))
			{
				SceneWidget->LoadSceneFromFilePath(PickedPath);
			}
		}
		if (ImGui::IsKeyPressed(ImGuiKey_S, false))
		{
			FString PickedPath;
			if (SaveSceneFileDialog(PickedPath))
			{
				SceneWidget->SaveSceneToFilePath(PickedPath);
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
	RenderViewMenu();
	RenderEditMenu();
	RenderHelpMenu();

	if (PlayStreamWidget)
	{
		PlayStreamWidget->Render(DeltaTime);
	}

	ImGui::EndMainMenuBar();
}

void FEditorToolbarWidget::RenderFilesMenu()
{
	if (!ImGui::BeginMenu("Files"))
	{
		return;
	}

	if (SceneWidget)
	{
		if (ImGui::MenuItem("New Scene", "Ctrl+N"))
		{
			SceneWidget->NewScene();
		}
		if (ImGui::MenuItem("Load Scene", "Ctrl+O"))
		{
			FString PickedPath;
			if (OpenSceneFileDialog(PickedPath))
			{
				SceneWidget->LoadSceneFromFilePath(PickedPath);
			}
		}
		if (ImGui::MenuItem("Save Scene", "Ctrl+S"))
		{
			FString PickedPath;
			if (SaveSceneFileDialog(PickedPath))
			{
				SceneWidget->SaveSceneToFilePath(PickedPath);
			}
		}

		ImGui::Separator();

		if (ImGui::MenuItem("Reload Asset From Disk"))
		{
			SceneWidget->RefreshSceneAndAssets();
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
		ImGui::Separator();
		ImGui::MenuItem("Reload Asset From Disk", nullptr, false, false);
		ImGui::MenuItem("Open Asset Folder", nullptr, false, false);
	}

	ImGui::EndMenu();
}

void FEditorToolbarWidget::RenderViewMenu()
{
	if (!ImGui::BeginMenu("View"))
	{
		return;
	}

	if (bShowConsole) ImGui::MenuItem("Console", nullptr, bShowConsole);
	if (bShowControl) ImGui::MenuItem("Control Panel", nullptr, bShowControl);
	if (bShowProperty) ImGui::MenuItem("Property", nullptr, bShowProperty);
	if (bShowSceneManager) ImGui::MenuItem("Scene Manager", nullptr, bShowSceneManager);
	if (bShowMaterialEditor) ImGui::MenuItem("Material Editor", nullptr, bShowMaterialEditor);
	if (bShowStatProfiler) ImGui::MenuItem("Stat Profiler", nullptr, bShowStatProfiler);

	if (ViewportOverlayWidget)
	{
		bool bShowViewportSettings = ViewportOverlayWidget->IsViewportSettingsVisible();
		if (ImGui::MenuItem("Viewport Settings", nullptr, bShowViewportSettings))
		{
			ViewportOverlayWidget->SetViewportSettingsVisible(!bShowViewportSettings);
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
