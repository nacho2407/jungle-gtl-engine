#include "Editor.h"

#include "Viewport/EditorViewportClient.h"
#include "Camera/ViewportCamera.h"

#include "Asset/AssetManager.h"
#include "Asset/Texture2DAsset.h"
#include "Core/Path.h"
#include "Engine/Component/Core/SceneComponent.h"
#include "Engine/EngineStatics.h"
#include "Engine/Game/Actor.h"
#include "Renderer/RenderAsset/TextureResource.h"
#include "SceneIO/SceneSerializer.h"
#include "Panel/ConsolePanel.h"
#include "Panel/ContentBrowserPanel.h"
#include "Panel/ControlPanel.h"
#include "Panel/OutlinerPanel.h"
#include "Panel/PanelManager.h"
#include "Panel/PropertiesPanel.h"
#include "Panel/ShortcutsPanel.h"

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <imgui_internal.h>

#include <algorithm>
#include <array>
#include <cstdint>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>

#pragma comment(lib, "Comdlg32.lib")

namespace
{
#ifdef IMGUI_HAS_DOCK
    constexpr ImGuiDockNodeFlags RootDockSpaceFlags = ImGuiDockNodeFlags_PassthruCentralNode;
#endif

    constexpr char    AboutPopupId[] = "About##EditorAbout";
    constexpr float   RestoredSideGutter = 3.0f;
    constexpr float   RestoredBottomGutter = 3.0f;
    constexpr ImU32   GutterColor = IM_COL32(46, 46, 48, 255);
    constexpr wchar_t SceneFileDialogFilter[] =
        L"Scene Files (*.Scene)\0*.Scene\0All Files (*.*)\0*.*\0";

    struct FEditorGutterMetrics
    {
        float Left = 0.0f;
        float Right = 0.0f;
        float Bottom = 0.0f;
    };

    FEditorGutterMetrics GetDockSpaceGutterMetrics(const IEditorChromeHost* Host)
    {
        const bool bIsMaximized = Host != nullptr && Host->IsWindowMaximized();
        if (bIsMaximized)
        {
            return {};
        }

        FEditorGutterMetrics Metrics;
        Metrics.Left = RestoredSideGutter;
        Metrics.Right = RestoredSideGutter;
        Metrics.Bottom = RestoredBottomGutter;
        return Metrics;
    }

    void DrawDockSpaceGutters(ImGuiViewport* Viewport, const FEditorGutterMetrics& Metrics)
    {
        if (Viewport == nullptr)
        {
            return;
        }

        const float Top = FEditorChrome::TitleBarHeight;
        const float ContentTop = Viewport->Pos.y + Top;
        const float ContentBottom = Viewport->Pos.y + Viewport->Size.y;
        if (ContentBottom <= ContentTop)
        {
            return;
        }

        ImDrawList* DrawList = ImGui::GetBackgroundDrawList(Viewport);
        if (DrawList == nullptr)
        {
            return;
        }

        if (Metrics.Left > 0.0f)
        {
            DrawList->AddRectFilled(ImVec2(Viewport->Pos.x, ContentTop),
                                    ImVec2(Viewport->Pos.x + Metrics.Left, ContentBottom),
                                    GutterColor);
        }

        if (Metrics.Right > 0.0f)
        {
            DrawList->AddRectFilled(
                ImVec2(Viewport->Pos.x + Viewport->Size.x - Metrics.Right, ContentTop),
                ImVec2(Viewport->Pos.x + Viewport->Size.x, ContentBottom), GutterColor);
        }

        if (Metrics.Bottom > 0.0f)
        {
            DrawList->AddRectFilled(
                ImVec2(Viewport->Pos.x, Viewport->Pos.y + Viewport->Size.y - Metrics.Bottom),
                ImVec2(Viewport->Pos.x + Viewport->Size.x, Viewport->Pos.y + Viewport->Size.y),
                GutterColor);
        }
    }

    FString BuildPanelCommandId(const FPanelDescriptor& Descriptor)
    {
        return "panel.toggle." + std::to_string(Descriptor.PanelType.hash_code());
    }

    HWND GetEditorOwnerWindow(const IEditorChromeHost* Host)
    {
        return static_cast<HWND>(Host != nullptr ? Host->GetNativeWindowHandle() : nullptr);
    }

    std::filesystem::path NormalizeSceneFilePath(const std::filesystem::path& FilePath)
    {
        if (FilePath.empty())
        {
            return {};
        }

        if (FilePath.has_extension() && _wcsicmp(FilePath.extension().c_str(), L".Scene") == 0)
        {
            return FilePath;
        }

        std::filesystem::path NormalizedPath = FilePath;
        NormalizedPath.replace_extension(L".Scene");
        return NormalizedPath;
    }

    FString PathToUtf8String(const std::filesystem::path& Path)
    {
        const std::u8string Utf8Path = Path.u8string();
        return FString(reinterpret_cast<const char*>(Utf8Path.data()), Utf8Path.size());
    }

    FWString Utf8ToWide(const FString& InText)
    {
        if (InText.empty())
        {
            return {};
        }

        const int RequiredSize = MultiByteToWideChar(CP_UTF8, 0, InText.c_str(),
                                                     static_cast<int>(InText.size()), nullptr, 0);
        if (RequiredSize <= 0)
        {
            return {};
        }

        FWString OutText(static_cast<size_t>(RequiredSize), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, InText.c_str(), static_cast<int>(InText.size()),
                            OutText.data(), RequiredSize);
        return OutText;
    }

    FString WideToUtf8(const FWString& InText)
    {
        if (InText.empty())
        {
            return {};
        }

        const int RequiredSize =
            WideCharToMultiByte(CP_UTF8, 0, InText.c_str(), static_cast<int>(InText.size()),
                                nullptr, 0, nullptr, nullptr);
        if (RequiredSize <= 0)
        {
            return {};
        }

        FString OutText(static_cast<size_t>(RequiredSize), '\0');
        WideCharToMultiByte(CP_UTF8, 0, InText.c_str(), static_cast<int>(InText.size()),
                            OutText.data(), RequiredSize, nullptr, nullptr);
        return OutText;
    }

    bool ShowOpenSceneFileDialog(const IEditorChromeHost*     Host,
                                 const std::filesystem::path& InitialDirectory,
                                 std::filesystem::path&       OutSelectedPath)
    {
        std::array<wchar_t, 1024> FileBuffer{};
        const FWString            InitialDirectoryPath =
            InitialDirectory.empty() ? FWString() : InitialDirectory.wstring();

        OPENFILENAMEW Dialog = {};
        Dialog.lStructSize = sizeof(Dialog);
        Dialog.hwndOwner = GetEditorOwnerWindow(Host);
        Dialog.lpstrFilter = SceneFileDialogFilter;
        Dialog.lpstrFile = FileBuffer.data();
        Dialog.nMaxFile = static_cast<DWORD>(FileBuffer.size());
        Dialog.lpstrInitialDir =
            InitialDirectoryPath.empty() ? nullptr : InitialDirectoryPath.c_str();
        Dialog.lpstrDefExt = L"Scene";
        Dialog.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

        if (!GetOpenFileNameW(&Dialog))
        {
            const DWORD ExtendedError = CommDlgExtendedError();
            if (ExtendedError != 0)
            {
                UE_LOG(FEditor, ELogVerbosity::Error,
                       "Open scene dialog failed with error code %lu.",
                       static_cast<unsigned long>(ExtendedError));
            }
            return false;
        }

        OutSelectedPath = std::filesystem::path(FileBuffer.data());
        return true;
    }

    bool ShowSaveSceneFileDialog(const IEditorChromeHost*     Host,
                                 const std::filesystem::path& InitialDirectory,
                                 const std::filesystem::path& CurrentScenePath,
                                 std::filesystem::path&       OutSelectedPath)
    {
        std::array<wchar_t, 1024> FileBuffer{};

        const std::filesystem::path DefaultPath =
            CurrentScenePath.empty() ? (InitialDirectory / L"Untitled.Scene") : CurrentScenePath;
        const FWString DefaultPathText = DefaultPath.wstring();
        wcsncpy_s(FileBuffer.data(), FileBuffer.size(), DefaultPathText.c_str(), _TRUNCATE);

        const FWString InitialDirectoryPath =
            InitialDirectory.empty() ? FWString() : InitialDirectory.wstring();

        OPENFILENAMEW Dialog = {};
        Dialog.lStructSize = sizeof(Dialog);
        Dialog.hwndOwner = GetEditorOwnerWindow(Host);
        Dialog.lpstrFilter = SceneFileDialogFilter;
        Dialog.lpstrFile = FileBuffer.data();
        Dialog.nMaxFile = static_cast<DWORD>(FileBuffer.size());
        Dialog.lpstrInitialDir =
            InitialDirectoryPath.empty() ? nullptr : InitialDirectoryPath.c_str();
        Dialog.lpstrDefExt = L"Scene";
        Dialog.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_OVERWRITEPROMPT;

        if (!GetSaveFileNameW(&Dialog))
        {
            const DWORD ExtendedError = CommDlgExtendedError();
            if (ExtendedError != 0)
            {
                UE_LOG(FEditor, ELogVerbosity::Error,
                       "Save scene dialog failed with error code %lu.",
                       static_cast<unsigned long>(ExtendedError));
            }
            return false;
        }

        OutSelectedPath = NormalizeSceneFilePath(std::filesystem::path(FileBuffer.data()));
        return true;
    }

    void ShowStatToggleDialog(const FEditorViewportClient& ViewportClient, EViewportStatFlags InStat)
    {
        switch (InStat)
        {
        case EViewportStatFlags::FPS:
            if (ViewportClient.IsStatEnabled(EViewportStatFlags::FPS))
            {
                UE_LOG(FEditor, ELogVerbosity::Log, "viewport stat enabled: fps");
            }
            else
            {
                UE_LOG(FEditor, ELogVerbosity::Log, "viewport stat disabled: fps");
            }
            break;
        case EViewportStatFlags::Memory:
            if (ViewportClient.IsStatEnabled(EViewportStatFlags::Memory))
            {
                UE_LOG(FEditor, ELogVerbosity::Log, "viewport stat enabled: memory");
            }
            else
            {
                UE_LOG(FEditor, ELogVerbosity::Log, "viewport stat disabled: memory");
            }
            break;
        case EViewportStatFlags::None:
            UE_LOG(FEditor, ELogVerbosity::Log, "all viewport stats disabled");
            break;
        }
    }

    FEditorViewportClient* GetActiveStatViewportClient(FEditor* Editor)
    {
        if (Editor == nullptr)
        {
            return nullptr;
        }

        if (FWindowOverlayManager* OverlayManager = Editor->GetWindowOverlayManager())
        {
            if (FEditorViewportPanel* FocusedPanel = OverlayManager->GetLastFocusedPanel())
            {
                if (FocusedPanel->ViewportClient != nullptr)
                {
                    return FocusedPanel->ViewportClient;
                }
            }
        }

        return &Editor->GetViewportClient();
    }

    /**
     * @brief 주어진 도킹 노드 트리에서 중앙 도킹 노드(게임 월드가 그려지는 중앙 패널)를 재귀적으로 탐색하여 반환합니다.
     * 중앙 노드가 항상 트리의 루트에 위치한다는 보장이 없기 때문에 탐색할 필요가 있습니다.
     * ChildNodes[0], ChildNodes[1] 접근은 internal 구조라 imgui_internal.h가 필요합니다.
     */
    ImGuiDockNode* FindCentralDockNode(ImGuiDockNode* Node)
    {
        if (Node == nullptr)
        {
            return nullptr;
        }

        if (Node->IsCentralNode())
        {
            return Node;
        }

        if (ImGuiDockNode* Found = FindCentralDockNode(Node->ChildNodes[0]))
        {
            return Found;
        }

        if (ImGuiDockNode* Found = FindCentralDockNode(Node->ChildNodes[1]))
        {
            return Found;
        }

        return nullptr;
    }

    /**
     * @brief 주어진 위치와 크기를 기반으로 FViewportAvailableArea 구조체를 생성하는 헬퍼
     */
    FViewportAvailableArea MakeViewportAreaFromRect(const ImVec2& Pos, const ImVec2& Size, bool bValid = true)
    {
        FViewportAvailableArea Area;
        Area.X = Pos.x;
        Area.Y = Pos.y;
        Area.Width = Size.x;
        Area.Height = Size.y;
        Area.bValid = bValid;
        return Area;
    }

    void ApplyInitialDockLayoutIfNeeded(ImGuiID DockSpaceId, const ImVec2& DockSpaceSize)
    {
        ImGuiDockNode* RootNode = ImGui::DockBuilderGetNode(DockSpaceId);
        if (RootNode == nullptr)
        {
            return;
        }

        const bool bHasExistingSplit =
            RootNode->ChildNodes[0] != nullptr || RootNode->ChildNodes[1] != nullptr;
        const bool bHasExistingWindows = RootNode->Windows.Size > 0;
        if (bHasExistingSplit || bHasExistingWindows)
        {
            return;
        }

        // imgui.ini가 아직 없는 첫 실행에서만 기본 패널 배치를 한 번 만듭니다.
        ImGui::DockBuilderRemoveNode(DockSpaceId);
        ImGui::DockBuilderAddNode(DockSpaceId,
                                  RootDockSpaceFlags | ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(DockSpaceId, DockSpaceSize);

        ImGuiID MainNode = DockSpaceId;
        ImGuiID RightNode = 0;
        ImGuiID RightTopNode = 0;
        ImGuiID RightBottomNode = 0;

        ImGui::DockBuilderSplitNode(MainNode, ImGuiDir_Right, 0.25f, &RightNode, &MainNode);
        ImGui::DockBuilderSplitNode(RightNode, ImGuiDir_Up, 0.4f, &RightTopNode,
                                    &RightBottomNode);

        // 중앙은 WindowOverlayManager가 실제 viewport 작업 영역으로 사용하므로 비워 둡니다.
        // 우측은 Unreal 스타일에 맞게 Outliner를 위, Details를 아래에 둡니다.
        ImGui::DockBuilderDockWindow("Outliner", RightTopNode);
        ImGui::DockBuilderDockWindow("Details", RightBottomNode);
        ImGui::DockBuilderFinish(DockSpaceId);
    }
} // namespace

void FEditor::Create()
{
    //  LOG
    GLog = &LogBuffer;

    //  TODO : Viewport Client
    EditorContext.Editor = this;
    EditorContext.ContentIndex = &ContentIndex;
    ContentIndex.Refresh();

    ViewportClient.Create();
    ViewportClient.SetEditorContext(&EditorContext);
    GlobalInputContext.SetNavigationController(&ViewportClient.GetNavigationController());

    GlobalInputController.SetEditorContext(&EditorContext);
    GlobalInputController.SetSelectionController(&ViewportClient.GetSelectionController());
    GlobalInputRouter.AddContext(&GlobalInputContext);

    // 메뉴 시스템은 command 등록과 배치 등록을 분리해서 초기화합니다.
    MenuRegistry.Clear();
    RegisterDefaultCommands();
    RegisterDefaultMenus();

    PanelManager = new FPanelManager();
    PanelManager->Initialize(&EditorContext);
    // 새 패널이 등록되면 Window 메뉴 항목도 함께 자동 등록합니다.
    PanelManager->SetPanelDescriptorRegisteredCallback([this](const FPanelDescriptor& Descriptor)
                                                       { RegisterWindowPanelCommand(Descriptor); });
    PanelManager->RegisterPanelInstance<FConsolePanel>(&LogBuffer);
    PanelManager->RegisterPanelType<FContentBrowserPanel>();
    PanelManager->RegisterPanelInstance<FControlPanel>();
    PanelManager->RegisterPanelInstance<FOutlinerPanel>();
    PanelManager->RegisterPanelInstance<FPropertiesPanel>();
    PanelManager->RegisterPanelInstance<FShortcutsPanel>();

    // PanelManager->RegisterPanelInstance<FSamplePanel>(&LogBuffer);

    //  TODO : Gizmo

    //  TEMP SCENE
    CurScene = new FScene();
    ViewportClient.SetScene(CurScene);
    GlobalInputController.SetScene(CurScene);

    UE_LOG(FEditor, ELogVerbosity::Log, "Hello Probopass!");
    EditorContext.Scene = CurScene;

    WindowOverlayManager = new FWindowOverlayManager();
    WindowOverlayManager->SetEditorContext(&EditorContext);
    WindowOverlayManager->SetScene(CurScene);
    FEditorViewportPanel* EditorPanel = new FEditorViewportPanel();
    EditorPanel->ViewportClient = &ViewportClient;
    WindowOverlayManager->GetViewportPanels().push_back(EditorPanel);

    // Share the primary ViewportClient's selection controller with all sub-panels
    // so there is only one SelectedActors array — eliminates stale-pointer crashes on actor delete.
    WindowOverlayManager->SetSharedSelectionController(&ViewportClient.GetSelectionController());

    WindowOverlayManager->SetViewportLayout(EViewportLayout::Single);

    // SubViewports and Splitter controls
    OverlayInputContext = new FViewportOverlayInputContext(WindowOverlayManager);
    GlobalInputRouter.AddContext(OverlayInputContext);

    LoadEditorSettings();
}

void FEditor::Release()
{
    SaveEditorSettings();
    ViewportClient.Release();
    AboutImageResource = nullptr;
    bAttemptedAboutImageLoad = false;

    if (PanelManager != nullptr)
    {
        PanelManager->Shutdown();
        delete PanelManager;
        PanelManager = nullptr;
    }

    delete CurScene;
    CurScene = nullptr;
    GlobalInputContext.SetNavigationController(nullptr);
    GlobalInputController.SetScene(nullptr);
    GlobalInputController.SetSelectionController(nullptr);
    GlobalInputController.SetEditorContext(nullptr);
    EditorContext.Scene = nullptr;
    EditorContext.ContentIndex = nullptr;

    MenuRegistry.Clear();
    ChromeHost = nullptr;
    EditorChrome.SetHost(nullptr);

    if (GLog == &LogBuffer)
    {
        GLog = nullptr;
    }

    delete OverlayInputContext;
    OverlayInputContext = nullptr;

    if (WindowOverlayManager)
    {
        WindowOverlayManager->Release();
        delete WindowOverlayManager;
        WindowOverlayManager = nullptr;
    }
}

void FEditor::Initialize()
{
    if (CurScene == nullptr)
    {
        CurScene = new FScene();
        ViewportClient.SetScene(CurScene);
        GlobalInputController.SetScene(CurScene);
        EditorContext.Scene = CurScene;
    }
}

void FEditor::SetChromeHost(IEditorChromeHost* InChromeHost)
{
    ChromeHost = InChromeHost;
    EditorChrome.SetHost(InChromeHost);
}

void FEditor::SetRuntimeServices(FD3D11RHI* InRHI, UAssetManager* InAssetManager)
{
    EditorContext.RHI = InRHI;
    EditorContext.AssetManager = InAssetManager;
    AboutImageResource = nullptr;
    bAttemptedAboutImageLoad = false;
    EnsureAboutImageLoaded();
    ResolveSceneAssetReferences(CurScene);
}

void FEditor::LoadEditorSettings()
{
    FEditorSettingsData SettingsData;
    SettingsData.GridSpacing = UEngineStatics::GridSpacing;
    SettingsData.CameraMoveSpeed = ViewportClient.GetNavigationController().GetMoveSpeed();
    SettingsData.CameraRotationSpeed = ViewportClient.GetNavigationController().GetRotationSpeed();
    SettingsData.ContentBrowserLeftPaneWidth = EditorContext.ContentBrowserLeftPaneWidth;

    FString                         ErrorMessage;
    const EEditorSettingsLoadResult LoadResult =
        PersistentSettings.Load(SettingsData, &ErrorMessage);

    if (LoadResult == EEditorSettingsLoadResult::Missing)
    {
        return;
    }

    if (LoadResult == EEditorSettingsLoadResult::InvalidFormat)
    {
        UE_LOG(FEditor, ELogVerbosity::Error, "Invalid editor.ini format: %s",
               ErrorMessage.c_str());
        return;
    }

    if (LoadResult == EEditorSettingsLoadResult::IOError)
    {
        UE_LOG(FEditor, ELogVerbosity::Error, "Failed to read editor.ini: %s",
               ErrorMessage.c_str());
        return;
    }

    UEngineStatics::GridSpacing = FMath::Clamp(SettingsData.GridSpacing, 1.0f, 1000.0f);
    ViewportClient.GetNavigationController().SetMoveSpeed(SettingsData.CameraMoveSpeed);
    ViewportClient.GetNavigationController().SetRotationSpeed(SettingsData.CameraRotationSpeed);
    EditorContext.ContentBrowserLeftPaneWidth =
        std::max(SettingsData.ContentBrowserLeftPaneWidth, 120.0f);

    if (WindowOverlayManager != nullptr)
    {
        WindowOverlayManager->SetNavigationValues(SettingsData.CameraMoveSpeed,
                                                  SettingsData.CameraRotationSpeed);
    }
}

void FEditor::SaveEditorSettings() const
{
    FEditorSettingsData SettingsData;
    SettingsData.GridSpacing = FMath::Clamp(UEngineStatics::GridSpacing, 1.0f, 1000.0f);
    SettingsData.CameraMoveSpeed = ViewportClient.GetNavigationController().GetMoveSpeed();
    SettingsData.CameraRotationSpeed = ViewportClient.GetNavigationController().GetRotationSpeed();
    SettingsData.ContentBrowserLeftPaneWidth =
        std::max(EditorContext.ContentBrowserLeftPaneWidth, 120.0f);
    PersistentSettings.Save(SettingsData);
}

void FEditor::MarkSceneClean() { SceneDocument.bDirty = false; }

std::filesystem::path FEditor::GetSceneDirectory() const
{
    return FPaths::Combine(FPaths::AppContentDir(), L"Scenes");
}

bool FEditor::SaveScene()
{
    CaptureCameraState();
    if (!SceneDocument.CurrentScenePath.empty())
    {
        return SaveSceneToPath(SceneDocument.CurrentScenePath, true);
    }

    SaveSceneAs();
    return false;
}

void FEditor::SaveSceneAs()
{
    std::filesystem::path       SelectedPath;
    const std::filesystem::path InitialDirectory =
        !SceneDocument.CurrentScenePath.empty() ? SceneDocument.CurrentScenePath.parent_path()
                                                : GetSceneDirectory();

    if (!ShowSaveSceneFileDialog(ChromeHost, InitialDirectory, SceneDocument.CurrentScenePath,
                                 SelectedPath))
    {
        SceneDocument.DeferredAction.Reset();
        return;
    }

    if (!SaveSceneToPath(SelectedPath, true))
    {
        SceneDocument.DeferredAction.Reset();
        return;
    }

    const FDeferredSceneAction DeferredAction = SceneDocument.DeferredAction;
    SceneDocument.DeferredAction.Reset();
    ExecuteDeferredSceneAction(DeferredAction);
}

void FEditor::RequestOpenScene()
{
    std::filesystem::path       SelectedPath;
    const std::filesystem::path InitialDirectory =
        !SceneDocument.CurrentScenePath.empty() ? SceneDocument.CurrentScenePath.parent_path()
                                                : GetSceneDirectory();

    if (!ShowOpenSceneFileDialog(ChromeHost, InitialDirectory, SelectedPath))
    {
        return;
    }

    const FDeferredSceneAction Action{.Type = EDeferredSceneActionType::OpenScene,
                                      .ScenePath = SelectedPath};
    if (!ConfirmProceedWithDirtyScene(Action))
    {
        return;
    }

    LoadSceneFromPath(SelectedPath);
}

void FEditor::PerformNewScene()
{
    ReplaceCurrentScene(std::make_unique<FScene>());
    SceneDocument.CurrentScenePath.clear();
    MarkSceneClean();
}

void FEditor::PerformClearScene()
{
    if (CurScene == nullptr)
    {
        ReplaceCurrentScene(std::make_unique<FScene>());
        return;
    }

    const TArray<AActor*>* SceneActors = CurScene->GetActors();
    const bool             bHadActors = SceneActors != nullptr && !SceneActors->empty();
    ViewportClient.GetSelectionController().ClearSelection();
    CurScene->Clear();

    if (bHadActors || !SceneDocument.CurrentScenePath.empty())
    {
        MarkSceneDirty();
    }
}

bool FEditor::SaveSceneToPath(const std::filesystem::path& FilePath, bool bUpdateCurrentPath)
{
    if (CurScene == nullptr)
    {
        UE_LOG(FEditor, ELogVerbosity::Error, "No scene is available to save.");
        return false;
    }

    FString ErrorMessage;
    if (!FSceneSerializer::SaveToFile(*CurScene, CameraState, FilePath, &ErrorMessage))
    {
        UE_LOG(FEditor, ELogVerbosity::Error, "Failed to save scene: %s", ErrorMessage.c_str());
        return false;
    }

    if (bUpdateCurrentPath)
    {
        SceneDocument.CurrentScenePath = FilePath;
    }
    MarkSceneClean();

    UE_LOG(FEditor, ELogVerbosity::Log, "Saved scene: %s", PathToUtf8String(FilePath).c_str());
    return true;
}

bool FEditor::LoadSceneFromPath(const std::filesystem::path& FilePath)
{
    FString                 ErrorMessage;
    std::unique_ptr<FScene> LoadedScene =
        FSceneDeserializer::LoadFromFile(FilePath, &CameraState, &ErrorMessage);
    if (!LoadedScene)
    {
        UE_LOG(FEditor, ELogVerbosity::Error, "Failed to load scene: %s", ErrorMessage.c_str());
        return false;
    }

    ReplaceCurrentScene(std::move(LoadedScene));
    ApplyCameraState();
    SceneDocument.CurrentScenePath = FilePath;
    MarkSceneClean();

    UE_LOG(FEditor, ELogVerbosity::Log, "Loaded scene: %s", PathToUtf8String(FilePath).c_str());
    return true;
}

void FEditor::CaptureCameraState()
{
    const FViewportCamera& Cam = ViewportClient.GetCamera();
    CameraState.Location    = Cam.GetLocation();
    CameraState.Rotation    = Cam.GetRotation();
    CameraState.FOV         = Cam.GetFOV();
    CameraState.NearPlane   = Cam.GetNearPlane();
    CameraState.FarPlane    = Cam.GetFarPlane();
    CameraState.OrthoHeight = Cam.GetOrthoHeight();
}

void FEditor::ApplyCameraState()
{
    FViewportCamera& Cam = ViewportClient.GetCamera();
    Cam.SetLocation(CameraState.Location);
    Cam.SetRotation(CameraState.Rotation);
    Cam.SetFOV(CameraState.FOV);
    Cam.SetNearPlane(CameraState.NearPlane);
    Cam.SetFarPlane(CameraState.FarPlane);
    Cam.SetOrthoHeight(CameraState.OrthoHeight);
    ViewportClient.GetNavigationController().ResetTargetLocation(CameraState.Location);
}

void FEditor::ReplaceCurrentScene(std::unique_ptr<FScene> NewScene)
{
    ViewportClient.GetSelectionController().ClearSelection();

    delete CurScene;
    CurScene = NewScene.release();
    if (CurScene == nullptr)
    {
        CurScene = new FScene();
    }

    ViewportClient.SetScene(CurScene);
    GlobalInputController.SetScene(CurScene);
    EditorContext.Scene = CurScene;
    EditorContext.SelectedObject = nullptr;
    EditorContext.SelectedActors.clear();
    if (WindowOverlayManager)
    {
        WindowOverlayManager->SetScene(CurScene);
    }
    ResolveSceneAssetReferences(CurScene);
}

void FEditor::ResolveActorAssetReferences(AActor* Actor)
{
    if (Actor == nullptr || EditorContext.AssetManager == nullptr)
    {
        return;
    }

    for (Engine::Component::USceneComponent* Component : Actor->GetOwnedComponents())
    {
        if (Component != nullptr)
        {
            Component->ResolveAssetReferences(EditorContext.AssetManager);
        }
    }
}

void FEditor::ResolveSceneAssetReferences(FScene* Scene)
{
    if (Scene == nullptr || EditorContext.AssetManager == nullptr)
    {
        return;
    }

    const TArray<AActor*>* SceneActors = Scene->GetActors();
    if (SceneActors == nullptr)
    {
        return;
    }

    for (AActor* Actor : *SceneActors)
    {
        ResolveActorAssetReferences(Actor);
    }
}

void FEditor::Tick(float DeltaTime, Engine::ApplicationCore::FInputSystem* InputSystem)
{
    using namespace Engine::ApplicationCore;

    EditorContext.DeltaTime = DeltaTime;
    FInputEvent        Event;
    const FInputState& InputState = InputSystem->GetInputState();

    while (InputSystem->PollEvent(Event))
    {
        GlobalInputRouter.RouteEvent(Event, InputState);
    }

    // Tick every panel's viewport client uniformly.
    if (WindowOverlayManager)
    {
        for (FEditorViewportPanel* Panel : WindowOverlayManager->GetViewportPanels())
        {
            if (Panel && Panel->ViewportClient)
            {
                Panel->ViewportClient->Tick(DeltaTime, InputState);
            }
        }
    }

    if (PanelManager != nullptr)
    {
        PanelManager->Tick(DeltaTime);
    }

    if (CurScene)
    {
        CurScene->Tick(DeltaTime);
    }

    //BuildRenderData();
}

void FEditor::OnWindowResized(float Width, float Height)
{
    if (Width <= 0 || Height <= 0)
    {
        return;
    }

    WindowHeight = Height;
    WindowWidth = Width;
    EditorContext.WindowWidth = Width;
    EditorContext.WindowHeight = Height;

    if (WindowOverlayManager != nullptr)
    {
        WindowOverlayManager->SetWindowDimension(static_cast<uint32>(Width),
                                                 static_cast<uint32>(Height));
    }
}

void FEditor::CreateNewScene()
{
    const FDeferredSceneAction Action{.Type = EDeferredSceneActionType::NewScene};
    if (!ConfirmProceedWithDirtyScene(Action))
    {
        return;
    }

    PerformNewScene();
}

bool FEditor::SaveCurrentSceneToDisk() { return SaveScene(); }

void FEditor::RequestSaveSceneAs() { SaveSceneAs(); }

void FEditor::RequestOpenSceneDialog() { RequestOpenScene(); }

void FEditor::RequestAboutPopUp() { RequestAboutPopup(); }

bool FEditor::SaveSceneAsPath(const std::filesystem::path& FilePath)
{
    return SaveSceneToPath(FilePath, true);
}

bool FEditor::OpenSceneFromPath(const std::filesystem::path& FilePath)
{
    return LoadSceneFromPath(FilePath);
}

bool FEditor::CanDeleteSelectedActors() const
{
    return GlobalInputController.CanDeleteSelectedActors();
}

bool FEditor::DeleteSelectedActors() { return GlobalInputController.DeleteSelectedActors(); }

void FEditor::RefreshContentIndex() { ContentIndex.Refresh(); }

std::filesystem::path FEditor::GetDefaultSceneDirectory() const { return GetSceneDirectory(); }

void FEditor::ClearScene()
{
    const FDeferredSceneAction Action{.Type = EDeferredSceneActionType::ClearScene};
    if (!ConfirmProceedWithDirtyScene(Action))
    {
        return;
    }

    PerformClearScene();
}

bool FEditor::RequestCloseEditor()
{
    const FDeferredSceneAction Action{.Type = EDeferredSceneActionType::CloseEditor};
    return ConfirmProceedWithDirtyScene(Action);
}

void FEditor::SetSelectedObject(UObject* InSelectedObject)
{
    EditorContext.SelectedActors.clear();
    if (AActor* SelectedActor = Cast<AActor>(InSelectedObject))
    {
        EditorContext.SelectedActors.push_back(SelectedActor);
    }
    else if (auto* SelectedComponent = Cast<Engine::Component::USceneComponent>(InSelectedObject))
    {
        if (AActor* OwnerActor = SelectedComponent->GetOwnerActor())
        {
            EditorContext.SelectedActors.push_back(OwnerActor);
        }
    }

    EditorContext.SelectedObject = InSelectedObject;
    ViewportClient.SyncSelectionFromContext();
}

void FEditor::AddActorToScene(AActor* InActor, bool bSelectActor)
{
    if (InActor == nullptr)
    {
        return;
    }

    if (CurScene == nullptr)
    {
        delete InActor;
        return;
    }

    CurScene->AddActor(InActor);
    ResolveActorAssetReferences(InActor);
    MarkSceneDirty();

    if (bSelectActor)
    {
        ViewportClient.GetSelectionController().SelectActor(InActor, ESelectionMode::Replace);
    }
}

void FEditor::MarkSceneDirty() { SceneDocument.bDirty = true; }

void FEditor::EnsureAboutImageLoaded()
{
    if (AboutImageResource != nullptr || bAttemptedAboutImageLoad ||
        EditorContext.AssetManager == nullptr)
    {
        return;
    }

    bAttemptedAboutImageLoad = true;

    const std::filesystem::path ImagePath =
        FPaths::Combine(FPaths::AppRoot(), L"Content\\Texture\\Logo\\copass.png");

    FAssetLoadParams LoadParams;
    LoadParams.ExplicitType = EAssetType::Texture;

    UAsset* LoadedAsset = EditorContext.AssetManager->Load(ImagePath.wstring(), LoadParams);
    UTexture2DAsset* TextureAsset = Cast<UTexture2DAsset>(LoadedAsset);
    if (TextureAsset == nullptr || TextureAsset->GetResource() == nullptr ||
        TextureAsset->GetSRV() == nullptr)
    {
        UE_LOG(FEditor, ELogVerbosity::Warning, "Failed to load About image: %s",
               PathToUtf8String(ImagePath).c_str());
        return;
    }

    AboutImageResource = TextureAsset->GetResource();
}

void FEditor::RequestAboutPopup()
{
    bRequestOpenAboutPopup = true;
    bAboutPopupOpen = true;
}

bool FEditor::ConfirmProceedWithDirtyScene(const FDeferredSceneAction& Action)
{
    if (!SceneDocument.bDirty)
    {
        return true;
    }

    FString ActionLabel = "continue";
    switch (Action.Type)
    {
    case EDeferredSceneActionType::NewScene:
        ActionLabel = "create a new scene";
        break;
    case EDeferredSceneActionType::ClearScene:
        ActionLabel = "clear the scene";
        break;
    case EDeferredSceneActionType::OpenScene:
        ActionLabel = "open another scene";
        break;
    case EDeferredSceneActionType::CloseEditor:
        ActionLabel = "close the editor";
        break;
    default:
        break;
    }

    const FWString Message =
        L"The current scene has unsaved changes.\n\nDo you want to save before you " +
        Utf8ToWide(ActionLabel) + L"?";
    const HWND OwnerWindow =
        static_cast<HWND>(ChromeHost != nullptr ? ChromeHost->GetNativeWindowHandle() : nullptr);
    const int DialogResult = MessageBoxW(OwnerWindow, Message.c_str(), L"Unsaved Scene",
                                         MB_ICONWARNING | MB_YESNOCANCEL | MB_SETFOREGROUND);

    if (DialogResult == IDCANCEL)
    {
        return false;
    }

    if (DialogResult == IDNO)
    {
        return true;
    }

    if (!SceneDocument.CurrentScenePath.empty())
    {
        return SaveSceneToPath(SceneDocument.CurrentScenePath, true);
    }

    SceneDocument.DeferredAction = Action;
    SaveSceneAs();
    return false;
}

void FEditor::ExecuteDeferredSceneAction(FDeferredSceneAction Action)
{
    if (Action.Type == EDeferredSceneActionType::None)
    {
        return;
    }

    switch (Action.Type)
    {
    case EDeferredSceneActionType::NewScene:
        PerformNewScene();
        break;

    case EDeferredSceneActionType::ClearScene:
        PerformClearScene();
        break;

    case EDeferredSceneActionType::OpenScene:
        if (!Action.ScenePath.empty())
        {
            LoadSceneFromPath(Action.ScenePath);
        }
        break;

    case EDeferredSceneActionType::CloseEditor:
        if (ChromeHost != nullptr)
        {
            ChromeHost->CloseWindow();
        }
        break;

    default:
        break;
    }
}

void FEditor::RegisterDefaultCommands()
{
    // 여기서는 "무엇을 실행할지"만 등록하고, 메뉴 어디에 둘지는 RegisterDefaultMenus에서 정합니다.
    MenuRegistry.RegisterCommand(
        FEditorCommandDefinition{.CommandId = "file.new_scene",
                                 .Label = L"New Scene",
                                 .ShortcutLabel = "Ctrl+N",
                                 .Execute = [this]() { CreateNewScene(); }});

    MenuRegistry.RegisterCommand(
        FEditorCommandDefinition{.CommandId = "file.open_scene",
                                 .Label = L"Open Scene",
                                 .ShortcutLabel = "Ctrl+O",
                                 .Execute = [this]() { RequestOpenScene(); }});

    MenuRegistry.RegisterCommand(FEditorCommandDefinition{.CommandId = "file.save_scene",
                                                          .Label = L"Save Scene",
                                                          .ShortcutLabel = "Ctrl+S",
                                                          .Execute = [this]() { SaveScene(); }});

    MenuRegistry.RegisterCommand(FEditorCommandDefinition{.CommandId = "file.save_scene_as",
                                                          .Label = L"Save Scene as..",
                                                          .ShortcutLabel = "Ctrl+Shift+S",
                                                          .Execute = [this]() { SaveSceneAs(); }});

    MenuRegistry.RegisterCommand(FEditorCommandDefinition{.CommandId = "file.clear_scene",
                                                          .Label = L"Clear Scene",
                                                          .Execute = [this]() { ClearScene(); }});

    MenuRegistry.RegisterCommand(FEditorCommandDefinition{.CommandId = "file.exit",
                                                          .Label = L"Exit",
                                                          .ShortcutLabel = "Alt+F4",
                                                          .Execute = [this]()
                                                          {
                                                              if (ChromeHost != nullptr)
                                                              {
                                                                  ChromeHost->CloseWindow();
                                                              }
                                                          }});

    MenuRegistry.RegisterCommand(FEditorCommandDefinition{.CommandId = "edit.undo",
                                                          .Label = L"Undo",
                                                          .ShortcutLabel = "Ctrl+Z",
                                                          .CanExecute = []() { return false; }});

    MenuRegistry.RegisterCommand(FEditorCommandDefinition{.CommandId = "edit.redo",
                                                          .Label = L"Redo",
                                                          .ShortcutLabel = "Ctrl+Y",
                                                          .CanExecute = []() { return false; }});

    MenuRegistry.RegisterCommand(FEditorCommandDefinition{
        .CommandId = "edit.delete_selection",
        .Label = L"Delete Selection",
        .ShortcutLabel = "Delete",
        .Execute =
            [this]()
        {
            // 1. 선택된 객체가 컴포넌트인지 확인합니다.
            if (auto* SelectedComp =
                    Cast<Engine::Component::USceneComponent>(EditorContext.SelectedObject))
            {
                AActor* Owner = SelectedComp->GetOwnerActor();
                // RootComponent가 아닌 일반 컴포넌트일 경우 컴포넌트만 삭제합니다.
                if (Owner != nullptr && SelectedComp != Owner->GetRootComponent())
                {
                    Owner->RemoveOwnedComponent(SelectedComp);

                    // 삭제 후 부모 액터를 선택 상태로 만듭니다.
                    SetSelectedObject(Owner);
                    MarkSceneDirty();
                    return;
                }
            }

            // 2. 컴포넌트가 아니거나 Root일 경우 기존 액터 삭제 로직을 수행합니다.
            GlobalInputController.DeleteSelectedActors();
        },
        .CanExecute =
            [this]()
        {
            // 컴포넌트가 선택되었거나, 삭제 가능한 액터가 있을 때 실행 가능합니다.
            if (Cast<Engine::Component::USceneComponent>(EditorContext.SelectedObject))
                return true;
            return GlobalInputController.CanDeleteSelectedActors();
        }});

    MenuRegistry.RegisterCommand(FEditorCommandDefinition{.CommandId = "edit.preferences",
                                                          .Label = L"Preferences (Not Ready)",
                                                          .CanExecute = []() { return false; }});

    MenuRegistry.RegisterCommand(FEditorCommandDefinition{
        .CommandId = "tool.content_browser",
        .Label = L"Content Browser",
        .Execute =
            [this]()
        {
            if (PanelManager == nullptr)
            {
                return;
            }

            FPanelOpenRequest Request;
            Request.PanelType = std::type_index(typeid(FContentBrowserPanel));
            Request.OpenPolicy = EPanelOpenPolicy::FocusIfOpenElseCreate;
            PanelManager->OpenPanel(Request);
        },
        .CanExecute = [this]() { return PanelManager != nullptr; }});

    MenuRegistry.RegisterCommand(FEditorCommandDefinition{.CommandId = "tool.build",
                                                          .Label = L"Build (Not Ready)",
                                                          .CanExecute = []() { return false; }});

    MenuRegistry.RegisterCommand(
        FEditorCommandDefinition{.CommandId = "help.shortcuts",
                                 .Label = L"Shortcuts",
                                 .Execute =
                                     [this]()
                                 {
                                     if (PanelManager == nullptr)
                                     {
                                         return;
                                     }

                                     FPanelOpenRequest Request;
                                     Request.PanelType = std::type_index(typeid(FShortcutsPanel));
                                     Request.OpenPolicy = EPanelOpenPolicy::FocusIfOpenElseCreate;
                                     PanelManager->OpenPanel(Request);
                                 },
                                 .CanExecute = [this]() { return PanelManager != nullptr; }});

    MenuRegistry.RegisterCommand(
        FEditorCommandDefinition{.CommandId = "help.about",
                                 .Label = L"About",
                                 .ShortcutLabel = "F1",
                                 .Execute = [this]() { RequestAboutPopup(); }});
}

void FEditor::RegisterDefaultMenus()
{
    // 여기서는 이미 등록된 command를 어떤 메뉴/순서에 배치할지만 정의합니다.
    MenuRegistry.RegisterMenuItem(FEditorMenuEntryDefinition{
        .MainMenu = EEditorMainMenu::File, .CommandId = "file.new_scene", .Order = 0});
    MenuRegistry.RegisterMenuItem(FEditorMenuEntryDefinition{
        .MainMenu = EEditorMainMenu::File, .CommandId = "file.open_scene", .Order = 10});
    MenuRegistry.RegisterMenuItem(FEditorMenuEntryDefinition{
        .MainMenu = EEditorMainMenu::File, .CommandId = "file.save_scene", .Order = 20});
    MenuRegistry.RegisterMenuItem(FEditorMenuEntryDefinition{
        .MainMenu = EEditorMainMenu::File, .CommandId = "file.save_scene_as", .Order = 30});
    MenuRegistry.RegisterMenuItem(FEditorMenuEntryDefinition{
        .MainMenu = EEditorMainMenu::File, .CommandId = "file.clear_scene", .Order = 40});
    MenuRegistry.RegisterMenuSeparator(EEditorMainMenu::File, {}, 50);
    MenuRegistry.RegisterMenuItem(FEditorMenuEntryDefinition{
        .MainMenu = EEditorMainMenu::File, .CommandId = "file.exit", .Order = 60});

    MenuRegistry.RegisterMenuItem(FEditorMenuEntryDefinition{
        .MainMenu = EEditorMainMenu::Edit, .CommandId = "edit.undo", .Order = 0});
    MenuRegistry.RegisterMenuItem(FEditorMenuEntryDefinition{
        .MainMenu = EEditorMainMenu::Edit, .CommandId = "edit.redo", .Order = 10});
    MenuRegistry.RegisterMenuItem(FEditorMenuEntryDefinition{
        .MainMenu = EEditorMainMenu::Edit, .CommandId = "edit.delete_selection", .Order = 20});
    MenuRegistry.RegisterMenuSeparator(EEditorMainMenu::Edit, {}, 30);
    MenuRegistry.RegisterMenuItem(FEditorMenuEntryDefinition{
        .MainMenu = EEditorMainMenu::Edit, .CommandId = "edit.preferences", .Order = 40});

    MenuRegistry.RegisterMenuItem(FEditorMenuEntryDefinition{
        .MainMenu = EEditorMainMenu::Tool, .CommandId = "tool.content_browser", .Order = 0});
    MenuRegistry.RegisterMenuItem(FEditorMenuEntryDefinition{
        .MainMenu = EEditorMainMenu::Tool, .CommandId = "tool.build", .Order = 10});

    MenuRegistry.RegisterMenuItem(FEditorMenuEntryDefinition{
        .MainMenu = EEditorMainMenu::Help, .CommandId = "help.shortcuts", .Order = 0});
    MenuRegistry.RegisterMenuItem(FEditorMenuEntryDefinition{
        .MainMenu = EEditorMainMenu::Help, .CommandId = "help.about", .Order = 10});
}

void FEditor::RegisterWindowPanelCommand(const FPanelDescriptor& Descriptor)
{
    if (!Descriptor.bShowInWindowMenu)
    {
        return;
    }

    // PanelDescriptor를 Window 메뉴용 체크형 command로 변환합니다.
    const FString         CommandId = BuildPanelCommandId(Descriptor);
    const std::type_index PanelType = Descriptor.PanelType;

    MenuRegistry.RegisterCommand(
        FEditorCommandDefinition{.CommandId = CommandId,
                                 .Label = Descriptor.DisplayName,
                                 .Execute =
                                     [this, PanelType]()
                                 {
                                     if (PanelManager == nullptr)
                                     {
                                         return;
                                     }

                                     FPanelOpenRequest Request;
                                     Request.PanelType = PanelType;
                                     PanelManager->TogglePanel(Request);
                                 },
                                 .CanExecute = [this]() { return PanelManager != nullptr; },
                                 .IsChecked =
                                     [this, PanelType]()
                                 {
                                     if (PanelManager == nullptr)
                                     {
                                         return false;
                                     }

                                     FPanelOpenRequest Request;
                                     Request.PanelType = PanelType;
                                     if (IPanel* Panel = PanelManager->FindPanel(Request))
                                     {
                                         return Panel->IsOpen();
                                     }

                                     return false;
                                 },
                                 .bCheckable = true});

    MenuRegistry.RegisterMenuItem(
        FEditorMenuEntryDefinition{.MainMenu = EEditorMainMenu::Window,
                                   .SubmenuPath = Descriptor.WindowMenuPath,
                                   .CommandId = CommandId,
                                   .Order = Descriptor.WindowMenuOrder});
}

void FEditor::DrawAboutPopup()
{
    if (bRequestOpenAboutPopup)
    {
        ImGui::OpenPopup(AboutPopupId);
        bRequestOpenAboutPopup = false;
    }

    EnsureAboutImageLoaded();
    ImGui::SetNextWindowSize(ImVec2(520.0f, 0.0f), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal(AboutPopupId, &bAboutPopupOpen, ImGuiWindowFlags_AlwaysAutoResize))
    {
        static const std::array<FWString, 4> Contributors = {
            L"강명호",
            L"김연하",
            L"김형도",
            L"양현석",
        };

        static const std::array<FWString, 4> ContributorSummaries = {
            L"에디터 UI, 패널, 도구 기능, 에셋 연결",
            L"렌더러, 스프라이트·텍스트 렌더링, ShowFlags",
            L"기즈모 조작, 중심 하이라이트, 다중 회전",
            L"엔진 루프, 뷰포트 입력, 카메라, 스냅핑",
        };

        static const std::array<FWString, 4> ContributorDisplayNames = {
            L"\uAC15\uBA85\uD638",
            L"\uAE40\uC5F0\uD558",
            L"\uAE40\uD615\uB3C4",
            L"\uC591\uD604\uC11D",
        };

        static const std::array<FWString, 4> ContributorDisplaySummaries = {
            L"Editor UI, panels, tools, and asset integration",
            L"Renderer, sprite/text rendering, and show flags",
            L"Gizmo interaction, center highlight, and multi-rotation",
            L"Engine loop, viewport input, camera, and snapping",
        };

        ImGui::TextUnformatted("CO-PASS Engine");
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(185, 185, 190, 255));
        ImGui::TextWrapped("Level editor and rendering sandbox for the CO-PASS project.");
        ImGui::PopStyleColor();
        ImGui::Separator();
        ImGui::TextUnformatted("Version: Internal Prototype");
#ifdef _DEBUG
        ImGui::Text("Build: Debug | %s | commit %s", __DATE__, "1ddf265");
#else
        ImGui::Text("Build: Release | %s | commit %s", __DATE__, "1ddf265");
#endif
        ImGui::TextUnformatted("Tech Stack: C++, Win32, Direct3D 11, ImGui");
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(185, 185, 190, 255));
        ImGui::TextWrapped("Includes third-party software such as Dear ImGui and nlohmann/json.");
        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::SeparatorText("First Contributors");  
        ImGui::Spacing();

        for (size_t ContributorIndex = 0; ContributorIndex < ContributorDisplayNames.size();
             ++ContributorIndex)
        {
            const FString ContributorUtf8 = WideToUtf8(ContributorDisplayNames[ContributorIndex]);
            const FString SummaryUtf8 = WideToUtf8(ContributorDisplaySummaries[ContributorIndex]);
            ImGui::BulletText("%s", ContributorUtf8.c_str());
            ImGui::Indent();
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(185, 185, 190, 255));
            ImGui::TextWrapped("%s", SummaryUtf8.c_str());
            ImGui::PopStyleColor();
            ImGui::Unindent();
            ImGui::Spacing();
            }

            ImGui::Spacing();
            ImGui::SeparatorText("Second Contributors");
            ImGui::Spacing();

         static const std::array<FWString, 4> SecondContributorNames = {
             L"\uAE40\uAE30\uD6C8", // 김기훈
             L"\uAE40\uD615\uB3C4", // 김형도
             L"\uAE40\uD615\uC900", // 김형준
             L"\uC7A5\uBBFC\uC900", // 장민준
         };

         static const std::array<FWString, 4> SecondContributorSummaries = {
             L"Editor UI & Tools, Material editing and UV scroll",
             L"OBJ parser, Binary mesh serialization and Material editing",
             L"Multi-viewport architecture, Camera systems and Viewer core",
             L"Stat overlay, Object iterator, Memory tracker, Postprocess Outline",
         };

            for (size_t i = 0; i < SecondContributorNames.size(); ++i)
            {
            const FString NameUtf8 = WideToUtf8(SecondContributorNames[i]);
            const FString SummaryUtf8 = WideToUtf8(SecondContributorSummaries[i]);
            ImGui::BulletText("%s", NameUtf8.c_str());
            ImGui::Indent();
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(185, 185, 190, 255));
            ImGui::TextWrapped("%s", SummaryUtf8.c_str());
            ImGui::PopStyleColor();
            ImGui::Unindent();
            ImGui::Spacing();
            }

            ImGui::Spacing();

            if (AboutImageResource != nullptr && AboutImageResource->GetSRV() != nullptr &&            AboutImageResource->Width > 0 && AboutImageResource->Height > 0)
        {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            const float AvailableWidth = ImGui::GetContentRegionAvail().x;
            const float TargetWidth = std::min(AvailableWidth, 460.0f);
            const float AspectRatio = static_cast<float>(AboutImageResource->Height) /
                                      static_cast<float>(AboutImageResource->Width);
            const ImVec2 ImageSize(TargetWidth, TargetWidth * AspectRatio);
            if (AvailableWidth > ImageSize.x)
            {
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                                     (AvailableWidth - ImageSize.x) * 0.5f);
            }

            ImGui::Image(ImTextureRef(static_cast<ImTextureID>(
                             reinterpret_cast<uintptr_t>(AboutImageResource->GetSRV()))),
                         ImageSize);
        }

        ImGui::Spacing();
        {
            const char* CopyrightText = "Copyright (c) 2026 CO-PASS Engine Contributors";
            const float AvailableWidth = ImGui::GetContentRegionAvail().x;
            const float TextWidth = ImGui::CalcTextSize(CopyrightText).x;
            if (AvailableWidth > TextWidth)
            {
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (AvailableWidth - TextWidth) * 0.5f);
            }

            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(150, 150, 155, 255));
            ImGui::TextUnformatted(CopyrightText);
            ImGui::PopStyleColor();
        }

        ImGui::EndPopup();
    }
}

void FEditor::BuildSceneView()
{
    SceneView.SetViewMatrix(ViewportClient.GetCamera().GetViewMatrix());
    SceneView.SetProjectionMatrix(ViewportClient.GetCamera().GetProjectionMatrix());
    SceneView.SetViewLocation(ViewportClient.GetCamera().GetLocation());

    FViewportRect ViewRect;
    ViewRect.X = 0;
    ViewRect.Y = 0;
    ViewRect.Width = static_cast<int32>(WindowWidth);
    ViewRect.Height = static_cast<int32>(WindowHeight);

    SceneView.SetViewRect(ViewRect);
    SceneView.SetClipPlanes(ViewportClient.GetCamera().GetNearPlane(),
                            ViewportClient.GetCamera().GetFarPlane());
}

void FEditor::DrawRootDockSpace()
{
#ifdef IMGUI_HAS_DOCK
    if ((ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable) == 0)
    {
        ViewportAvailableArea = {};
        return;
    }

    ImGuiViewport* Viewport = ImGui::GetMainViewport();
    if (Viewport == nullptr)
    {
        ViewportAvailableArea = {};
        return;
    }

    const FEditorGutterMetrics GutterMetrics = GetDockSpaceGutterMetrics(ChromeHost);
    DrawDockSpaceGutters(Viewport, GutterMetrics);

    const float DockSpaceWidth = (Viewport->Size.x > (GutterMetrics.Left + GutterMetrics.Right))
                                     ? (Viewport->Size.x - GutterMetrics.Left - GutterMetrics.Right)
                                     : 0.0f;
    const float DockSpaceHeight =
        (Viewport->Size.y > (FEditorChrome::TitleBarHeight + GutterMetrics.Bottom))
            ? (Viewport->Size.y - FEditorChrome::TitleBarHeight - GutterMetrics.Bottom)
            : 0.0f;

    const float DockSpaceX = Viewport->Pos.x + GutterMetrics.Left;
    const float DockSpaceY = Viewport->Pos.y + FEditorChrome::TitleBarHeight;
    const float SafeDockSpaceWidth = (DockSpaceWidth > 0.0f) ? DockSpaceWidth : 1.0f;
    const float SafeDockSpaceHeight = (DockSpaceHeight > 0.0f) ? DockSpaceHeight : 1.0f;

    FViewportAvailableArea ComputedArea;
    if (DockSpaceWidth > 0.0f && DockSpaceHeight > 0.0f)
    {
        ComputedArea = MakeViewportAreaFromRect(ImVec2(DockSpaceX, DockSpaceY), ImVec2(DockSpaceWidth, DockSpaceHeight));
    }
    else
    {
        ComputedArea = MakeViewportAreaFromRect(ImVec2(DockSpaceX, DockSpaceY),
                                                ImVec2(SafeDockSpaceWidth, SafeDockSpaceHeight), false);
    }

    ImGui::SetNextWindowPos(ImVec2(DockSpaceX, DockSpaceY));
    ImGui::SetNextWindowSize(ImVec2(SafeDockSpaceWidth, SafeDockSpaceHeight));
    ImGui::SetNextWindowViewport(Viewport->ID);

    ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
                                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus |
                                   ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoSavedSettings;

    if ((RootDockSpaceFlags & ImGuiDockNodeFlags_PassthruCentralNode) != 0)
    {
        WindowFlags |= ImGuiWindowFlags_NoBackground;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    if (ImGui::Begin("##EditorRootDockSpace", nullptr, WindowFlags))
    {
        const ImGuiID DockSpaceId = ImGui::GetID("EditorRootDockSpace");
        ImGui::DockSpace(DockSpaceId, ImVec2(0.0f, 0.0f), RootDockSpaceFlags);

        ApplyInitialDockLayoutIfNeeded(DockSpaceId,
                                       ImVec2(SafeDockSpaceWidth, SafeDockSpaceHeight));

        ImGuiDockNode* RootNode = ImGui::DockBuilderGetNode(DockSpaceId);
        ImGuiDockNode* CentralNode = FindCentralDockNode(RootNode);

        if (CentralNode != nullptr)
        {
            ComputedArea = MakeViewportAreaFromRect(CentralNode->Pos, CentralNode->Size);
        }
    }

    ImGui::End();

    ViewportAvailableArea = ComputedArea;

    ImGui::PopStyleVar(3);
#endif
}

void FEditor::DrawPanel()
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();

    ImGui::NewFrame();
    // 매 프레임 현재 enabled/checked 상태를 평가해서 chrome이 소비할 메뉴 스냅샷을 만듭니다.
    const TArray<FEditorChromeMenu> ChromeMenus = MenuRegistry.BuildChromeMenus();
    DrawRootDockSpace();

    if (PanelManager != nullptr)
    {
        PanelManager->DrawPanels();
    }

    if (WindowOverlayManager != nullptr)
    {
        const FViewportAvailableArea& Area = GetViewportAvailableArea();
        if (Area.bValid)
        {
            WindowOverlayManager->SetViewportAvailableArea(Area.X, Area.Y, Area.Width, Area.Height);
        }

        for (FEditorViewportPanel* Panel : WindowOverlayManager->GetViewportPanels())
        {
            if (Panel != nullptr && Panel->ViewportClient != nullptr)
            {
                Panel->ViewportClient->DrawViewportOverlay(
                    ImVec2(Panel->PosX, Panel->PosY),
                    ImVec2(Panel->Width, Panel->Height));
            }
        }
    }
    else
    {
        ViewportClient.DrawViewportOverlay(
            ImVec2(static_cast<float>(ViewportClient.GetViewportOriginX()),
                   static_cast<float>(ViewportClient.GetViewportOriginY())),
            ImVec2(static_cast<float>(ViewportClient.GetCamera().GetWidth()),
                   static_cast<float>(ViewportClient.GetCamera().GetHeight())));
    }

    EditorChrome.Draw(ChromeMenus);
    DrawAboutPopup();

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void FEditor::BuildRenderData()
{
}

// 다중 뷰포트 머지 후에 active viewport를 토글하는 방식으로 변경 예정
void FEditor::ToggleActiveViewportStat(EViewportStatFlags StatFlag)
{
    if (FEditorViewportClient* ActiveViewportClient = GetActiveStatViewportClient(this))
    {
        ActiveViewportClient->ToggleStat(StatFlag);
        ShowStatToggleDialog(*ActiveViewportClient, StatFlag);
    }
}

void FEditor::ClearActiveViewportStats()
{
    bool bClearedAnyViewport = false;

    if (WindowOverlayManager != nullptr)
    {
        for (FEditorViewportPanel* Panel : WindowOverlayManager->GetViewportPanels())
        {
            if (Panel != nullptr && Panel->ViewportClient != nullptr)
            {
                Panel->ViewportClient->ClearAllStats();
                bClearedAnyViewport = true;
            }
        }
    }
    else
    {
        ViewportClient.ClearAllStats();
        bClearedAnyViewport = true;
    }

    if (bClearedAnyViewport)
    {
        ShowStatToggleDialog(ViewportClient, EViewportStatFlags::None);
    }
}
