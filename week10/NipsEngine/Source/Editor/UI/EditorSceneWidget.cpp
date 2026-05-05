#include "Editor/UI/EditorSceneWidget.h"

#include "Editor/EditorEngine.h"
#include "Editor/Viewport/EditorViewportClient.h"
#include "Editor/Viewport/ViewportLayout.h"
#include "Engine/Core/Common.h"
#include "GameFramework/WorldContext.h"
#include "GameFramework/PrimitiveActors.h"

#include "ImGui/imgui.h"
#include "Component/GizmoComponent.h"
#include "Serialization/SceneSaveManager.h"
#include "Serialization/PrefabManager.h"

#include <Windows.h>
#include <commdlg.h>
#include <algorithm>
#include <cctype>
#include <filesystem>

#include "Core/ResourceManager.h"

#define SEPARATOR(); ImGui::Spacing(); ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing(); ImGui::Spacing();

namespace
{
	FString TrimActorName(const FString& Name)
	{
		const auto First = std::find_if_not(Name.begin(), Name.end(), [](unsigned char Ch) { return std::isspace(Ch) != 0; });
		const auto Last = std::find_if_not(Name.rbegin(), Name.rend(), [](unsigned char Ch) { return std::isspace(Ch) != 0; }).base();
		if (First >= Last)
		{
			return "";
		}
		return FString(First, Last);
	}

	bool ParseActorNameNumber(const FString& Text, int32& OutNumber)
	{
		if (Text.empty())
		{
			return false;
		}

		int32 Value = 0;
		for (char Ch : Text)
		{
			if (!std::isdigit(static_cast<unsigned char>(Ch)))
			{
				return false;
			}
			Value = Value * 10 + (Ch - '0');
		}

		OutNumber = Value;
		return true;
	}

	bool SplitGeneratedActorNameSuffix(const FString& Name, FString& OutBaseName, int32& OutNumber)
	{
		const FString TrimmedName = TrimActorName(Name);
		if (TrimmedName.empty())
		{
			return false;
		}

		if (TrimmedName.back() == ')')
		{
			const size_t OpenParen = TrimmedName.rfind(" (");
			if (OpenParen != FString::npos && OpenParen + 2 < TrimmedName.size() - 1)
			{
				const FString NumberText = TrimmedName.substr(OpenParen + 2, TrimmedName.size() - OpenParen - 3);
				if (ParseActorNameNumber(NumberText, OutNumber))
				{
					OutBaseName = TrimActorName(TrimmedName.substr(0, OpenParen));
					return !OutBaseName.empty();
				}
			}
		}

		size_t NumberBegin = TrimmedName.size();
		while (NumberBegin > 0 && std::isdigit(static_cast<unsigned char>(TrimmedName[NumberBegin - 1])) != 0)
		{
			--NumberBegin;
		}

		if (NumberBegin == TrimmedName.size() || NumberBegin == 0 || TrimmedName[NumberBegin - 1] != '_')
		{
			return false;
		}

		if (ParseActorNameNumber(TrimmedName.substr(NumberBegin), OutNumber))
		{
			OutBaseName = TrimActorName(TrimmedName.substr(0, NumberBegin - 1));
			return !OutBaseName.empty();
		}
		return false;
	}

	FString StripGeneratedActorNameSuffixes(const FString& Name)
	{
		FString BaseName = TrimActorName(Name);
		for (;;)
		{
			FString NextBaseName;
			int32 IgnoredNumber = 0;
			if (!SplitGeneratedActorNameSuffix(BaseName, NextBaseName, IgnoredNumber))
			{
				return BaseName;
			}
			BaseName = NextBaseName;
		}
	}

	FString ToLowerCopy(FString Value)
	{
		std::transform(Value.begin(), Value.end(), Value.begin(), [](unsigned char Ch) { return static_cast<char>(std::tolower(Ch)); });
		return Value;
	}

	bool ContainsCaseInsensitive(const FString& Text, const char* Filter)
	{
		if (!Filter || Filter[0] == '\0')
		{
			return true;
		}
		return ToLowerCopy(Text).find(ToLowerCopy(Filter)) != FString::npos;
	}
}

void FEditorSceneWidget::Initialize(UEditorEngine* InEditorEngine)
{
	FEditorWidget::Initialize(InEditorEngine);
}

void FEditorSceneWidget::NewScene()
{
	if (!EditorEngine)
	{
		return;
	}

	if (!PromptSaveIfDirty())
	{
		return;
	}

	EditorEngine->GetMainPanel().ResetWidgetSelections();
	EditorEngine->NewScene();
	EditorEngine->ClearUndoHistory();
	strncpy_s(SceneName, IM_ARRAYSIZE(SceneName), "Untitled", _TRUNCATE);
	CurrentSceneFilePath.clear();
	bSceneDirty = false;
	EditorEngine->GetMainPanel().PushFooterLog("New level created");
	NewSceneNotificationTimer = common::constants::ImGui::NotificationTimer;
}

bool FEditorSceneWidget::SaveScene()
{
	if (CurrentSceneFilePath.empty())
	{
		FString PickedPath;
		if (!PromptSaveSceneAs(PickedPath))
		{
			return false;
		}
		return SaveSceneToFilePath(PickedPath);
	}

	return SaveSceneToFilePath(CurrentSceneFilePath);
}

void FEditorSceneWidget::LoadScene()
{
	if (!EditorEngine || SceneFiles.empty() || SelectedSceneIndex < 0)
	{
		return;
	}

	std::filesystem::path ScenePath = std::filesystem::path(FSceneSaveManager::GetSceneDirectory())
		/ (FPaths::ToWide(SceneFiles[SelectedSceneIndex]) + FSceneSaveManager::SceneExtension);
	LoadSceneFromFilePath(FPaths::ToUtf8(ScenePath.wstring()));
}

bool FEditorSceneWidget::SaveSceneToFilePath(const FString& FilePath)
{
	if (!EditorEngine)
	{
		return false;
	}

	std::filesystem::path TargetPath = std::filesystem::path(FPaths::ToWide(FilePath));
	const bool bNameOnlySave = !TargetPath.has_parent_path() && TargetPath.root_path().empty();
	if (TargetPath.extension().empty())
	{
		TargetPath += FSceneSaveManager::SceneExtension;
	}

	const FString FinalSceneName = FPaths::ToUtf8(TargetPath.stem().wstring());
	strncpy_s(SceneName, IM_ARRAYSIZE(SceneName), FinalSceneName.c_str(), _TRUNCATE);

	FWorldContext* Ctx = EditorEngine->GetWorldContextFromHandle(EditorEngine->GetActiveWorldHandle());
	if (!Ctx)
	{
		return false;
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
		return false;
	}

	const std::filesystem::path StoredPath = bNameOnlySave
		? (std::filesystem::path(FSceneSaveManager::GetSceneDirectory()) / (FPaths::ToWide(FinalSceneName) + FSceneSaveManager::SceneExtension))
		: TargetPath;
	SetCurrentScenePath(FPaths::ToUtf8(StoredPath.wstring()));
	bSceneDirty = false;
	EditorEngine->ClearUndoHistory();
	EditorEngine->GetMainPanel().PushFooterLog("Level saved");
	SceneSaveNotificationTimer = common::constants::ImGui::NotificationTimer;
	return true;
}

bool FEditorSceneWidget::LoadSceneFromFilePath(const FString& FilePath, bool bPromptSave)
{
	if (!EditorEngine)
	{
		return false;
	}

	if (bPromptSave && !PromptSaveIfDirty())
	{
		return false;
	}

	EditorEngine->GetMainPanel().ResetWidgetSelections();
	EditorEngine->ClearScene();
	FWorldContext LoadCtx;
	FEditorCameraState LoadedCam;
	//FSceneSaveManager::LoadSceneFromJSON(FilePath, LoadCtx, &LoadedCam);
	FSceneSaveManager::Load(FilePath, LoadCtx, &LoadedCam);
	if (LoadCtx.World)
	{
		EditorEngine->GetWorldList().push_back(LoadCtx);
		EditorEngine->SetActiveWorld(LoadCtx.ContextHandle);
		EditorEngine->ApplySpatialIndexMaintenanceSettings(LoadCtx.World);
	}
	EditorEngine->ResetViewport();

	// ResetViewport 가 카메라를 InitViewPos 로 초기화하므로 그 이후에 덮어씁니다
	if (LoadedCam.bValid)
	{
		if (FViewportCamera* Cam = EditorEngine->GetCamera())
		{
			Cam->SetLocation(LoadedCam.Location);
			Cam->SetRotation(FQuat(LoadedCam.Rotation));
			Cam->SetFOV(LoadedCam.FOV * (3.14159265358979f / 180.f));
			Cam->SetNearPlane(LoadedCam.NearClip);
			Cam->SetFarPlane(LoadedCam.FarClip);
			// ApplyCameraMode 가 설정한 TargetLocation(기본값)이 남아 있으면
			// 다음 Tick 에서 EditorWorldController 가 카메라를 되돌려 버리므로
			// 새 위치로 동기화한다.
			EditorEngine->GetViewportLayout().GetViewportClient(0)->SyncCameraTarget();
		}
	}

	SetCurrentScenePath(FilePath);
	bSceneDirty = false;
	EditorEngine->ClearUndoHistory();
	EditorEngine->GetMainPanel().PushFooterLog("Level loaded");
	SceneLoadNotificationTimer = common::constants::ImGui::NotificationTimer;
	return LoadCtx.World != nullptr;
}

void FEditorSceneWidget::RefreshSceneAndAssets()
{
	FResourceManager::Get().RefreshFromAssetDirectory(FPaths::ToUtf8(FPaths::AssetDirectoryPath()));
}

FString FEditorSceneWidget::GetCurrentSceneDisplayPath() const
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

bool FEditorSceneWidget::PromptSaveIfDirty()
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
	return SaveScene();
}

bool FEditorSceneWidget::PromptSaveSceneAs(FString& OutFilePath) const
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

bool FEditorSceneWidget::PromptSavePrefabAs(const AActor* Actor, FString& OutFilePath) const
{
	OutFilePath.clear();
	if (!Actor)
	{
		return false;
	}

	WCHAR FileBuffer[MAX_PATH] = {};
	std::filesystem::path PrefabDir(FPrefabManager::GetPrefabDirectory());
	PrefabDir = PrefabDir.lexically_normal();
	std::error_code CreateDirEc;
	std::filesystem::create_directories(PrefabDir, CreateDirEc);

	FString ActorName = Actor->GetName();
	if (ActorName.empty())
	{
		ActorName = Actor->GetTypeInfo() ? Actor->GetTypeInfo()->name : "Actor";
	}

	const std::wstring DefaultFile = (PrefabDir / (FPaths::ToWide(ActorName) + FPrefabManager::PrefabExtension)).wstring();
	const std::wstring InitialDir = PrefabDir.wstring();
	wcsncpy_s(FileBuffer, MAX_PATH, DefaultFile.c_str(), _TRUNCATE);

	OPENFILENAMEW DialogDesc = {};
	DialogDesc.lStructSize = sizeof(DialogDesc);
	DialogDesc.hwndOwner = EditorEngine && EditorEngine->GetWindow() ? EditorEngine->GetWindow()->GetHWND() : nullptr;
	DialogDesc.lpstrFilter = L"Prefab Files (*.prefab)\0*.prefab\0All Files (*.*)\0*.*\0";
	DialogDesc.lpstrFile = FileBuffer;
	DialogDesc.nMaxFile = MAX_PATH;
	DialogDesc.lpstrInitialDir = InitialDir.c_str();
	DialogDesc.lpstrDefExt = L"prefab";
	DialogDesc.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

	if (!GetSaveFileNameW(&DialogDesc))
	{
		return false;
	}

	OutFilePath = FPaths::ToUtf8(FileBuffer);
	return true;
}

void FEditorSceneWidget::SetCurrentScenePath(const FString& FilePath)
{
	CurrentSceneFilePath = FPaths::Normalize(FilePath);
	const FString FinalSceneName = FPaths::ToUtf8(std::filesystem::path(FPaths::ToWide(CurrentSceneFilePath)).stem().wstring());
	strncpy_s(SceneName, IM_ARRAYSIZE(SceneName), FinalSceneName.c_str(), _TRUNCATE);
}

void FEditorSceneWidget::Render(float DeltaTime)
{
    using namespace common::constants::ImGui;
    (void)DeltaTime;

    if (!EditorEngine) return;

    UWorld* World = EditorEngine->GetFocusedWorld();
    if (!World) return; // Early Exit: 전체 코드의 들여쓰기 깊이를 한 단계 줄임

    const TArray<AActor*>& Actors = World->GetActors();

    // LastClickedActorIndex 유효성 검사
    if (LastClickedActorIndex >= static_cast<int32>(Actors.size()))
    {
        LastClickedActorIndex = -1;
    }

    ImGui::SetNextWindowSize(ImVec2(400.0f, 350.0f), ImGuiCond_Once);
    ImGui::Begin("Outliner");

    TArray<int32> VisibleActorIndices;
    VisibleActorIndices.reserve(Actors.size());
    for (int32 ActorIndex = 0; ActorIndex < static_cast<int32>(Actors.size()); ++ActorIndex)
    {
        AActor* Actor = Actors[ActorIndex];
        if (!Actor)
        {
            continue;
        }

        FString ActorName = Actor->GetFName().ToString();
        if (ActorName.empty())
        {
            ActorName = Actor->GetTypeInfo()->name;
        }

        const FString SearchText = ActorName + " " + Actor->GetTypeInfo()->name;
        if (ContainsCaseInsensitive(SearchText, OutlinerSearchText))
        {
            VisibleActorIndices.push_back(ActorIndex);
        }
    }

    ImGui::Text("Actors (%d/%d)", static_cast<int32>(VisibleActorIndices.size()), static_cast<int32>(Actors.size()));
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##OutlinerSearch", "Search actors...", OutlinerSearchText, IM_ARRAYSIZE(OutlinerSearchText));
    ImGui::Separator();

    FSelectionManager& Selection = EditorEngine->GetSelectionManager();

    auto IsActorNameTaken = [&](AActor* TargetActor, const FString& CandidateName)
    {
        for (AActor* Actor : Actors)
        {
            if (!Actor || Actor == TargetActor)
            {
                continue;
            }
            if (Actor->GetFName() == FName(CandidateName))
            {
                return true;
            }
        }
        return false;
    };

    auto MakeUniqueActorName = [&](AActor* TargetActor, const FString& RequestedName)
    {
        const FString RequestedCleanName = TrimActorName(RequestedName);
        FString BaseName = StripGeneratedActorNameSuffixes(RequestedName);
        if (BaseName.empty())
        {
            BaseName = TargetActor ? TargetActor->GetTypeInfo()->name : "Actor";
        }

        if (!RequestedCleanName.empty() && !IsActorNameTaken(TargetActor, RequestedCleanName))
        {
            return RequestedCleanName;
        }

        int32 HighestSuffix = 0;
        for (AActor* Actor : Actors)
        {
            if (!Actor || Actor == TargetActor)
            {
                continue;
            }

            FString ExistingBaseName;
            int32 ExistingSuffix = 0;
            if (SplitGeneratedActorNameSuffix(Actor->GetFName().ToString(), ExistingBaseName, ExistingSuffix)
                && StripGeneratedActorNameSuffixes(ExistingBaseName) == BaseName)
            {
                HighestSuffix = std::max(HighestSuffix, ExistingSuffix);
            }
        }

        int32 Suffix = std::max(HighestSuffix + 1, 1);
        FString Candidate;
        do
        {
            Candidate = BaseName + "_" + std::to_string(Suffix++);
        }
        while (IsActorNameTaken(TargetActor, Candidate));
        return Candidate;
    };

    auto DeleteSelectedActors = [&]()
    {
        TArray<AActor*> ActorsToDelete = Selection.GetSelectedActors();
        if (ActorsToDelete.empty())
        {
            return;
        }

        if (EditorEngine->DeleteActors(ActorsToDelete) > 0)
        {
            LastClickedActorIndex = -1;
        }
    };

    auto RequestRenameActor = [&](AActor* RenameTarget)
    {
        if (!RenameTarget)
        {
            return;
        }

        PendingRenameActor = RenameTarget;
        const FString CurrentName = PendingRenameActor->GetFName().ToString();
        strncpy_s(RenameActorName, IM_ARRAYSIZE(RenameActorName), CurrentName.c_str(), _TRUNCATE);
        bOpenRenameActorPopup = true;
    };

    auto DrawOutlinerContextMenu = [&]()
    {
        const TArray<AActor*>& SelectedActors = Selection.GetSelectedActors();
        AActor* RenameTarget = SelectedActors.size() == 1 ? SelectedActors.front() : nullptr;

        ImGui::BeginDisabled(RenameTarget == nullptr);
        if (ImGui::MenuItem("Rename", "F2"))
        {
            RequestRenameActor(RenameTarget);
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();

        ImGui::Separator();
        ImGui::BeginDisabled(RenameTarget == nullptr);
        if (ImGui::MenuItem("Prefabication..."))
        {
            FString PrefabPath;
            if (PromptSavePrefabAs(RenameTarget, PrefabPath) && FPrefabManager::SaveActorPrefab(RenameTarget, PrefabPath))
            {
                EditorEngine->GetMainPanel().PushFooterLog("Prefab saved");
            }
            else
            {
                EditorEngine->GetMainPanel().PushFooterLog("Prefab save canceled or failed");
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();

        ImGui::Separator();
        if (ImGui::BeginMenu("Place Actor"))
        {
            if (EditorEngine->GetMainPanel().GetControlWidget().DrawPlaceActorMenu(FVector(0.0f, 0.0f, 0.0f), true))
            {
                LastClickedActorIndex = -1;
                World->RebuildSpatialIndex();
                MarkSceneDirty();
                EditorEngine->GetMainPanel().PushFooterLog("Actor placed from Outliner");
            }
            ImGui::EndMenu();
        }

        ImGui::Separator();
        ImGui::BeginDisabled(Selection.IsEmpty());
        if (ImGui::MenuItem("Delete", "Del"))
        {
            DeleteSelectedActors();
        }
        ImGui::EndDisabled();
    };

    // ctrl 클릭, ctrl + shift 클릭, shift 클릭, 기본 클릭 4가지 상태에 따라 각각 처리하는 람다 함수입니다.
    auto HandleActorSelection = [&](AActor* SelectedActor, int32 CurrentIndex)
    {
        const bool bShiftDown = ImGui::GetIO().KeyShift;
        const bool bCtrlDown  = ImGui::GetIO().KeyCtrl;

        // 기준점이 유효한지 확인 (이전 클릭 내역)
        const bool bValidRange = (LastClickedActorIndex >= 0) &&
                                 (LastClickedActorIndex < static_cast<int32>(Actors.size()));

        if (bCtrlDown && bShiftDown)
        {
            // 1. Ctrl + Shift + Click : 기존 선택을 유지한 채로 범위 안의 액터들을 '추가'
            if (bValidRange)
            {
                const int32 Start = std::min(LastClickedActorIndex, CurrentIndex);
                const int32 End   = std::max(LastClickedActorIndex, CurrentIndex);

                for (int32 i = Start; i <= End; ++i)
                {
                    if (AActor* RangeActor = Actors[i])
                    {
                        // 선택되어 있지 않은 경우에만 Toggle하여 '추가'되도록 보장
                        if (!Selection.IsSelected(RangeActor))
                        {
                            Selection.ToggleSelect(RangeActor);
                        }
                    }
                }
            }
            else
            {
                // 기준점이 없으면 단일 추가 선택으로 Fallback
                if (!Selection.IsSelected(SelectedActor)) Selection.ToggleSelect(SelectedActor);
                LastClickedActorIndex = CurrentIndex;
            }
        }
        else if (bShiftDown)
        {
            // 2. Shift + Click : 기존 선택을 모두 해제하고 새로운 범위만 선택
            if (bValidRange)
            {
                Selection.ClearSelection();

                const int32 Start = std::min(LastClickedActorIndex, CurrentIndex);
                const int32 End   = std::max(LastClickedActorIndex, CurrentIndex);

                for (int32 i = Start; i <= End; ++i)
                {
                    if (AActor* RangeActor = Actors[i])
                    {
                        Selection.ToggleSelect(RangeActor); 
                    }
                }
            }
            else
            {
                // 기준점이 없으면 단일 선택으로 Fallback
                Selection.Select(SelectedActor);
                LastClickedActorIndex = CurrentIndex;
            }
        }
        else if (bCtrlDown)
        {
            // 3. Ctrl + Click : 개별 액터 추가/해제 (토글)
            Selection.ToggleSelect(SelectedActor);
            LastClickedActorIndex = CurrentIndex; // 범위 선택의 새로운 기준점이 됨
        }
        else
        {
            // 4. Click : 단일 선택
            Selection.Select(SelectedActor);
            LastClickedActorIndex = CurrentIndex; // 범위 선택의 새로운 기준점이 됨
        }
    };

    // UI 렌더링 영역
    ImGui::BeginChild("ActorList", ImVec2(0, 0), ImGuiChildFlags_Borders);

    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
        && !ImGui::GetIO().WantTextInput
        && ImGui::GetIO().KeyCtrl
        && ImGui::IsKeyPressed(ImGuiKey_A, false))
    {
        Selection.ClearSelection();
        for (int32 ActorIndex : VisibleActorIndices)
        {
            if (AActor* Actor = Actors[ActorIndex])
            {
                Selection.AddSelect(Actor);
            }
        }
        LastClickedActorIndex = VisibleActorIndices.empty() ? -1 : VisibleActorIndices.front();
    }

    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
        && !ImGui::GetIO().WantTextInput
        && ImGui::IsKeyPressed(ImGuiKey_Delete, false))
    {
        const bool bHadSelection = !Selection.IsEmpty();
        DeleteSelectedActors();
        if (bHadSelection)
        {
            ImGui::EndChild();
            ImGui::End();
            return;
        }
    }

    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
        && !ImGui::GetIO().WantTextInput
        && ImGui::IsKeyPressed(ImGuiKey_F2, false))
    {
        const TArray<AActor*>& SelectedActors = Selection.GetSelectedActors();
        if (SelectedActors.size() == 1)
        {
            RequestRenameActor(SelectedActors.front());
        }
    }

    const ImGuiTableFlags TableFlags =
        ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_BordersOuterH |
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_SizingStretchProp;
    if (ImGui::BeginTable("##OutlinerActorTable", 2, TableFlags))
    {
        ImGui::TableSetupColumn("Item Label", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 150.0f);
        ImGui::TableHeadersRow();

        ImGuiListClipper Clipper;
        Clipper.Begin(static_cast<int>(VisibleActorIndices.size()));

        while (Clipper.Step())
        {
            for (int VisibleIndex = Clipper.DisplayStart; VisibleIndex < Clipper.DisplayEnd; VisibleIndex++)
            {
                const int32 i = VisibleActorIndices[VisibleIndex];
                AActor* Actor = Actors[i];
                if (!Actor) continue;

                FString ActorName = Actor->GetFName().ToString();
                if (ActorName.empty())
                {
                    ActorName = Actor->GetTypeInfo()->name;
                }
                const FString ActorType = Actor->GetTypeInfo()->name;

                ImGui::PushID(i);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);

                const bool bIsSelected = Selection.IsSelected(Actor);
                const bool bClicked = ImGui::Selectable(
                    ActorName.c_str(),
                    bIsSelected,
                    ImGuiSelectableFlags_SpanAllColumns);
                const bool bHovered = ImGui::IsItemHovered();
                const bool bRightClicked = ImGui::IsItemClicked(ImGuiMouseButton_Right);

                ImGui::TableSetColumnIndex(1);
                ImGui::TextDisabled("%s", ActorType.c_str());

                if (bClicked)
                {
                    HandleActorSelection(Actor, i);
                }
                if (bHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    Selection.Select(Actor);
                    LastClickedActorIndex = i;
                    FEditorViewportLayout& Layout = EditorEngine->GetViewportLayout();
                    const int32 FocusedViewportIndex = Layout.GetLastFocusedViewportIndex();
                    if (FEditorViewportClient* Client = Layout.GetViewportClient(FocusedViewportIndex))
                    {
                        Client->FocusSelection();
                    }
                }

                if (bRightClicked)
                {
                    if (!Selection.IsSelected(Actor))
                    {
                        Selection.Select(Actor);
                        LastClickedActorIndex = i;
                    }
                    bOpenOutlinerContextMenu = true;
                }

                ImGui::PopID();
            }
        }

        Clipper.End();
        ImGui::EndTable();
    }
    if (!ImGui::IsAnyItemHovered()
        && ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup)
        && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        bOpenOutlinerContextMenu = true;
    }
    if (bOpenOutlinerContextMenu)
    {
        ImGui::OpenPopup("##OutlinerContextMenu");
        bOpenOutlinerContextMenu = false;
    }
    if (ImGui::BeginPopup("##OutlinerContextMenu"))
    {
        DrawOutlinerContextMenu();
        ImGui::EndPopup();
    }

    if (bOpenRenameActorPopup)
    {
        ImGui::OpenPopup("Rename Actor");
        bOpenRenameActorPopup = false;
    }

    const bool bRenameTargetAlive = PendingRenameActor
        && std::find(Actors.begin(), Actors.end(), PendingRenameActor) != Actors.end();
    if (!bRenameTargetAlive)
    {
        PendingRenameActor = nullptr;
    }

    if (ImGui::BeginPopupModal("Rename Actor", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("Actor Name");
        ImGui::SetNextItemWidth(260.0f);
        const bool bCommitByEnter = ImGui::InputText("##RenameActorName", RenameActorName, IM_ARRAYSIZE(RenameActorName), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::Spacing();

        const bool bApplyClicked = ImGui::Button("Apply", ImVec2(90.0f, 0.0f));
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(90.0f, 0.0f)))
        {
            PendingRenameActor = nullptr;
            ImGui::CloseCurrentPopup();
        }

        if ((bCommitByEnter || bApplyClicked) && PendingRenameActor)
        {
            const FString UniqueName = MakeUniqueActorName(PendingRenameActor, RenameActorName);
            EditorEngine->CaptureUndoSnapshot("Rename Actor");
            PendingRenameActor->SetFName(FName(UniqueName));
            MarkSceneDirty();
            EditorEngine->GetMainPanel().PushFooterLog("Actor renamed");
            PendingRenameActor = nullptr;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
    ImGui::EndChild();
    ImGui::End(); // Begin("Outliner")에 대한 End
}
