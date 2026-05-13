#include "Editor/Scene/EditorSceneService.h"

#include "Editor/EditorEngine.h"
#include "Editor/Settings/ProjectSettings.h"
#include "Editor/Viewport/EditorViewportClient.h"
#include "Editor/Viewport/ViewportLayout.h"
#include "Core/Logging/Log.h"
#include "Core/Paths.h"
#include "GameFramework/WorldContext.h"
#include "Runtime/WindowsWindow.h"
#include "Serialization/SceneSaveManager.h"

#include <Windows.h>
#include <commdlg.h>
#include <algorithm>
#include <filesystem>

void FEditorSceneService::Initialize(UEditorEngine* InEditorEngine)
{
	EditorEngine = InEditorEngine;
}

FEditorSceneOperationResult FEditorSceneService::NewScene()
{
	if (!EditorEngine)
	{
		return MakeResult(false, "Editor is not initialized");
	}
	if (!PromptSaveIfDirty())
	{
		return MakeResult(false, "New scene canceled");
	}

	EditorEngine->GetMainPanel().ResetWidgetSelections();
	EditorEngine->NewScene();
	EditorEngine->GetUndoSystem().ClearAllHistory();
	strncpy_s(SceneName, sizeof(SceneName), "Untitled", _TRUNCATE);
	ClearCurrentScenePath();
	bSceneDirty = false;
	PushFooterLog("New level created");
	return MakeResult(true, "New level created");
}

FEditorSceneOperationResult FEditorSceneService::OpenScene(const FString& FilePath, bool bPromptSave)
{
	if (!EditorEngine)
	{
		return MakeResult(false, "Editor is not initialized", FilePath);
	}
	if (FilePath.empty())
	{
		return MakeResult(false, "Scene path is empty", FilePath);
	}
	if (bPromptSave && !PromptSaveIfDirty())
	{
		return MakeResult(false, "Open scene canceled", FilePath);
	}

	EditorEngine->GetMainPanel().ResetWidgetSelections();
	EditorEngine->ClearScene();

	FWorldContext LoadCtx;
	FEditorCameraState LoadedCam;
	FSceneSaveManager::Load(FilePath, LoadCtx, &LoadedCam);
	if (LoadCtx.World)
	{
		EditorEngine->GetWorldList().push_back(LoadCtx);
		EditorEngine->SetActiveWorld(LoadCtx.ContextHandle);
		EditorEngine->ApplySpatialIndexMaintenanceSettings(LoadCtx.World);

		EditorEngine->GetViewportLayout().Init(EditorEngine->GetWindow(), LoadCtx.World, LoadCtx.SelectionManager, EditorEngine);
		EditorEngine->GetViewportLayout().BuildViewportLayout(
			static_cast<int32>(EditorEngine->GetWindow()->GetWidth()),
			static_cast<int32>(EditorEngine->GetWindow()->GetHeight()));
	}

	EditorEngine->ResetViewport();

	if (LoadedCam.bValid)
	{
		if (FViewportCamera* Cam = EditorEngine->GetCamera())
		{
			Cam->SetLocation(LoadedCam.Location);
			Cam->SetRotation(FQuat(LoadedCam.Rotation));
			Cam->SetFOV(LoadedCam.FOV * (3.14159265358979f / 180.f));
			Cam->SetNearPlane(LoadedCam.NearClip);
			Cam->SetFarPlane(LoadedCam.FarClip);
			if (FEditorViewportClient* ViewportClient = EditorEngine->GetViewportLayout().GetViewportClient(0))
			{
				ViewportClient->SyncCameraTarget();
			}
		}
	}

	SetCurrentScenePath(FilePath);
	bSceneDirty = false;
	EditorEngine->GetUndoSystem().ClearAllHistory();
	PushFooterLog("Level loaded");
	return MakeResult(LoadCtx.World != nullptr, LoadCtx.World ? "Level loaded" : "Level load failed", FilePath);
}

FEditorSceneOperationResult FEditorSceneService::SaveScene()
{
	if (CurrentSceneFilePath.empty())
	{
		FString PickedPath;
		if (!PromptSaveSceneAs(PickedPath))
		{
			return MakeResult(false, "Save scene canceled");
		}
		return SaveSceneToFilePath(PickedPath);
	}
	return SaveSceneToFilePath(CurrentSceneFilePath);
}

FEditorSceneOperationResult FEditorSceneService::SaveSceneToFilePath(const FString& FilePath)
{
	if (!EditorEngine)
	{
		return MakeResult(false, "Editor is not initialized", FilePath);
	}
	if (FilePath.empty())
	{
		return MakeResult(false, "Scene path is empty", FilePath);
	}

	std::filesystem::path TargetPath = std::filesystem::path(FPaths::ToWide(FilePath));
	const bool bNameOnlySave = !TargetPath.has_parent_path() && TargetPath.root_path().empty();
	if (TargetPath.extension().empty())
	{
		TargetPath += FSceneSaveManager::SceneExtension;
	}

	const FString FinalSceneName = FPaths::ToUtf8(TargetPath.stem().wstring());
	strncpy_s(SceneName, sizeof(SceneName), FinalSceneName.c_str(), _TRUNCATE);

	FWorldContext* Ctx = EditorEngine->GetWorldContextFromHandle(EditorEngine->GetActiveWorldHandle());
	if (!Ctx)
	{
		return MakeResult(false, "Active world is missing", FilePath);
	}

	FEditorCameraState CamState;
	if (const FViewportCamera* Cam = EditorEngine->GetCamera())
	{
		CamState.Location = Cam->GetLocation();
		CamState.Rotation = FRotator(Cam->GetRotation());
		CamState.FOV = Cam->GetFOV() * (180.f / 3.14159265358979f);
		CamState.NearClip = Cam->GetNearPlane();
		CamState.FarClip = Cam->GetFarPlane();
		CamState.bValid = true;
	}

	const std::filesystem::path SavePath = bNameOnlySave
		? (std::filesystem::path(FSceneSaveManager::GetSceneDirectory()) / (FPaths::ToWide(FinalSceneName) + FSceneSaveManager::SceneExtension))
		: TargetPath;
	if (!FSceneSaveManager::SaveToFilePath(FPaths::ToUtf8(SavePath.wstring()), *Ctx, &CamState))
	{
		return MakeResult(false, "Level save failed", FilePath);
	}

	SetCurrentScenePath(FPaths::ToUtf8(SavePath.wstring()));
	bSceneDirty = false;
	EditorEngine->GetUndoSystem().ClearHistory();
	PushFooterLog("Level saved");
	return MakeResult(true, "Level saved", CurrentSceneFilePath);
}

FEditorSceneOperationResult FEditorSceneService::CreateSceneAsset(const FString& FilePath)
{
	if (!EditorEngine)
	{
		return MakeResult(false, "Editor is not initialized", FilePath);
	}
	const bool bSaved = EditorEngine->CreateDefaultSceneAsset(FilePath);
	return MakeResult(bSaved, bSaved ? "Scene asset created" : "Scene asset creation failed", FilePath);
}

FEditorSceneOperationResult FEditorSceneService::RestoreLastScene()
{
	if (!EditorEngine)
	{
		return MakeResult(false, "Editor is not initialized");
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
			FEditorSceneOperationResult Result = OpenScene(FPaths::ToUtf8(ScenePath.wstring()), false);
			if (Result.bSuccess)
			{
				return Result;
			}
			UE_LOG_WARNING("[ProjectSettings] Failed to load last scene: %s", StoredScenePath.c_str());
		}
		else
		{
			UE_LOG_WARNING("[ProjectSettings] Last scene path is invalid, opening New Scene: %s", StoredScenePath.c_str());
		}
	}

	return NewScene();
}

bool FEditorSceneService::PromptSaveIfDirty()
{
	if (!bSceneDirty)
	{
		return true;
	}

	HWND Owner = EditorEngine && EditorEngine->GetWindow() ? EditorEngine->GetWindow()->GetHWND() : nullptr;
	const int Result = MessageBoxW(
		Owner,
		L"Current level has unsaved changes. Save before continuing?",
		L"Unsaved Level",
		MB_ICONWARNING | MB_YESNOCANCEL | MB_DEFBUTTON1);

	if (Result == IDCANCEL)
	{
		return false;
	}
	if (Result == IDNO)
	{
		return true;
	}
	return SaveScene().bSuccess;
}

FString FEditorSceneService::GetCurrentSceneDisplayPath() const
{
	if (CurrentSceneFilePath.empty())
	{
		return bSceneDirty ? "Unsaved *" : "Unsaved";
	}

	std::filesystem::path SceneDir = std::filesystem::path(FSceneSaveManager::GetSceneDirectory()).lexically_normal();
	std::filesystem::path ScenePath = std::filesystem::path(FPaths::ToWide(CurrentSceneFilePath)).lexically_normal();
	std::error_code Ec;
	std::filesystem::path RelativePath = std::filesystem::relative(ScenePath, SceneDir.parent_path(), Ec);
	FString Display = Ec ? FPaths::ToUtf8(ScenePath.filename().wstring()) : FPaths::ToUtf8(RelativePath.wstring());
	std::replace(Display.begin(), Display.end(), '/', '\\');
	return bSceneDirty ? Display + " *" : Display;
}

bool FEditorSceneService::PromptSaveSceneAs(FString& OutFilePath) const
{
	OutFilePath.clear();

	WCHAR FileBuffer[MAX_PATH] = {};
	std::filesystem::path SceneDir(FSceneSaveManager::GetSceneDirectory());
	SceneDir = SceneDir.lexically_normal();
	if (!SceneDir.is_absolute())
	{
		SceneDir = std::filesystem::path(FPaths::ToAbsolute(SceneDir.wstring()));
	}
	SceneDir.make_preferred();
	std::error_code CreateDirEc;
	std::filesystem::create_directories(SceneDir, CreateDirEc);

	const std::wstring InitialDir = SceneDir.wstring();
	const std::wstring DefaultFile = (SceneDir / L"Untitled.Scene").wstring();
	wcsncpy_s(FileBuffer, MAX_PATH, DefaultFile.c_str(), _TRUNCATE);

	OPENFILENAMEW DialogDesc = {};
	DialogDesc.lStructSize = sizeof(DialogDesc);
	DialogDesc.hwndOwner = EditorEngine && EditorEngine->GetWindow() ? EditorEngine->GetWindow()->GetHWND() : nullptr;
	DialogDesc.lpstrFilter = L"Scene Files (*.Scene)\0*.Scene\0All Files (*.*)\0*.*\0";
	DialogDesc.lpstrFile = FileBuffer;
	DialogDesc.nMaxFile = MAX_PATH;
	DialogDesc.lpstrInitialDir = InitialDir.c_str();
	DialogDesc.lpstrDefExt = L"Scene";
	DialogDesc.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

	if (!GetSaveFileNameW(&DialogDesc))
	{
		return false;
	}

	OutFilePath = FPaths::ToUtf8(FileBuffer);
	return true;
}

void FEditorSceneService::SetCurrentScenePath(const FString& FilePath)
{
	CurrentSceneFilePath = FPaths::Normalize(FilePath);
	const FString FinalSceneName = FPaths::ToUtf8(std::filesystem::path(FPaths::ToWide(CurrentSceneFilePath)).stem().wstring());
	strncpy_s(SceneName, sizeof(SceneName), FinalSceneName.c_str(), _TRUNCATE);

	if (EditorEngine)
	{
		EditorEngine->CurrentScenePath = CurrentSceneFilePath;
	}

	FProjectSettings::Get().SetLastScenePath(CurrentSceneFilePath);
	FProjectSettings::Get().SaveToFile(FProjectSettings::GetDefaultSettingsPath());
}

void FEditorSceneService::ClearCurrentScenePath()
{
	CurrentSceneFilePath.clear();
	if (EditorEngine)
	{
		EditorEngine->CurrentScenePath.clear();
	}

	FProjectSettings::Get().SetLastScenePath("");
	FProjectSettings::Get().SaveToFile(FProjectSettings::GetDefaultSettingsPath());
}

void FEditorSceneService::PushFooterLog(const FString& Message) const
{
	if (EditorEngine && !Message.empty())
	{
		EditorEngine->GetNotificationService().Info(Message);
	}
}

FEditorSceneOperationResult FEditorSceneService::MakeResult(bool bSuccess, const FString& Message, const FString& ScenePath) const
{
	FEditorSceneOperationResult Result;
	Result.bSuccess = bSuccess;
	Result.Message = Message;
	Result.ScenePath = ScenePath;
	return Result;
}
