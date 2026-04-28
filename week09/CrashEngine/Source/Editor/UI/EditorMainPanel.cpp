// 에디터 영역의 세부 동작을 구현합니다.
#include "Editor/UI/EditorMainPanel.h"

#include "Editor/EditorEngine.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/Viewport/LevelEditorViewportClient.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Engine/Serialization/SceneSaveManager.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"

#include "Render/Renderer.h"
#include "Engine/Input/InputSystem.h"
#include "Component/CameraComponent.h"
#include "Platform/Paths.h"

#include <commdlg.h>
#include <filesystem>

namespace
{
void SaveEditorSettings()
{
    FEditorSettings::Get().SaveToFile(FEditorSettings::GetDefaultSettingsPath());
}

std::string OpenSceneFileDialog(bool bSave)
{
    wchar_t FileName[MAX_PATH] = L"";
    const std::filesystem::path SceneDir = FPaths::SceneDir();

    OPENFILENAMEW Ofn = {};
    Ofn.lStructSize = sizeof(OPENFILENAMEW);
    Ofn.lpstrFilter = L"Scene Files (*.scene)\0*.scene\0All Files (*.*)\0*.*\0";
    Ofn.lpstrFile = FileName;
    Ofn.nMaxFile = MAX_PATH;
    Ofn.lpstrDefExt = L"scene";
    Ofn.lpstrInitialDir = SceneDir.c_str();
    Ofn.Flags = bSave
                    ? (OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR)
                    : (OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR);

    const BOOL bResult = bSave ? GetSaveFileNameW(&Ofn) : GetOpenFileNameW(&Ofn);
    return bResult ? FPaths::FromWide(FileName) : std::string();
}

void SaveSceneFromEditor(UEditorEngine* EditorEngine)
{
    if (!EditorEngine)
    {
        return;
    }

    const std::string FilePath = OpenSceneFileDialog(true);
    if (FilePath.empty())
    {
        return;
    }

    EditorEngine->StopPlayInEditorImmediate();
    FWorldContext* Ctx = EditorEngine->GetWorldContextFromHandle(EditorEngine->GetActiveWorldHandle());
    if (!Ctx)
    {
        return;
    }

    UCameraComponent* PerspectiveCam = nullptr;
    for (FLevelEditorViewportClient* VC : EditorEngine->GetLevelViewportClients())
    {
        if (VC->GetRenderOptions().ViewportType == ELevelViewportType::Perspective ||
            VC->GetRenderOptions().ViewportType == ELevelViewportType::FreeOrthographic)
        {
            PerspectiveCam = VC->GetCamera();
            break;
        }
    }

    std::filesystem::path ScenePath = FPaths::ToPath(FilePath);
    FSceneSaveManager::SaveSceneAsJSON(FPaths::FromPath(ScenePath.stem()), *Ctx, PerspectiveCam);
}

void LoadSceneFromEditor(UEditorEngine* EditorEngine)
{
    if (!EditorEngine)
    {
        return;
    }

    const std::string FilePath = OpenSceneFileDialog(false);
    if (FilePath.empty())
    {
        return;
    }

    EditorEngine->StopPlayInEditorImmediate();
    EditorEngine->ClearScene();

    FWorldContext LoadCtx;
    FPerspectiveCameraData CamData;
    FSceneSaveManager::LoadSceneFromJSON(FilePath, LoadCtx, CamData);
    if (LoadCtx.World)
    {
        EditorEngine->GetWorldList().push_back(LoadCtx);
        EditorEngine->SetActiveWorld(LoadCtx.ContextHandle);
        EditorEngine->GetSelectionManager().SetWorld(LoadCtx.World);
        LoadCtx.World->WarmupPickingData();
    }
    EditorEngine->ResetViewport();

    if (CamData.bValid)
    {
        for (FLevelEditorViewportClient* VC : EditorEngine->GetLevelViewportClients())
        {
            if (VC->GetRenderOptions().ViewportType == ELevelViewportType::Perspective ||
                VC->GetRenderOptions().ViewportType == ELevelViewportType::FreeOrthographic)
            {
                if (UCameraComponent* Cam = VC->GetCamera())
                {
                    Cam->SetWorldLocation(CamData.Location);
                    Cam->SetRelativeRotation(CamData.Rotation);
                    FCameraState CS = Cam->GetCameraState();
                    CS.FOV = CamData.FOV;
                    CS.NearZ = CamData.NearClip;
                    CS.FarZ = CamData.FarClip;
                    Cam->SetCameraState(CS);
                }
                break;
            }
        }
    }
}
} // namespace

void FEditorMainPanel::Create(FWindowsWindow* InWindow, FRenderer& InRenderer, UEditorEngine* InEditorEngine)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& IO = ImGui::GetIO();
    IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    Window = InWindow;
    EditorEngine = InEditorEngine;

    IO.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\malgun.ttf", 17.0f, nullptr, IO.Fonts->GetGlyphRangesKorean());
    IO.FontGlobalScale = 1.00f;
    ImGui::GetStyle().ScaleAllSizes(1.00f);

    ImGui_ImplWin32_Init((void*)InWindow->GetHWND());
    ImGui_ImplDX11_Init(InRenderer.GetFD3DDevice().GetDevice(), InRenderer.GetFD3DDevice().GetDeviceContext());

    ConsolePanel.Initialize(InEditorEngine);
    ControlPanel.Initialize(InEditorEngine);
    DetailsPanel.Initialize(InEditorEngine);
    ScenePanel.Initialize(InEditorEngine);
    StatPanel.Initialize(InEditorEngine);
}

void FEditorMainPanel::Release()
{
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void FEditorMainPanel::Render(float DeltaTime)
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("New Scene"))
            {
                EditorEngine->NewScene();
            }
            if (ImGui::MenuItem("Load Scene..."))
            {
                LoadSceneFromEditor(EditorEngine);
            }
            if (ImGui::MenuItem("Save Scene..."))
            {
                SaveSceneFromEditor(EditorEngine);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit"))
        {
            FEditorSettings& Settings = FEditorSettings::Get();
            int32 CurrentPath = static_cast<int32>(Settings.RenderShadingPath);
            bool bChanged = false;

            if (ImGui::MenuItem("Deferred Shading", nullptr, CurrentPath == static_cast<int32>(ERenderShadingPath::Deferred)))
            {
                Settings.RenderShadingPath = ERenderShadingPath::Deferred;
                bChanged = true;
            }

            if (ImGui::MenuItem("Forward Shading", nullptr, CurrentPath == static_cast<int32>(ERenderShadingPath::Forward)))
            {
                Settings.RenderShadingPath = ERenderShadingPath::Forward;
                bChanged = true;
            }

            if (bChanged)
            {
                SaveEditorSettings();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Windows"))
        {
            FEditorSettings& S = FEditorSettings::Get();
            ImGui::MenuItem("Console", nullptr, &S.UI.bConsole);
            ImGui::MenuItem("Control Panel", nullptr, &S.UI.bControl);
            ImGui::MenuItem("Details", nullptr, &S.UI.bProperty);
            ImGui::MenuItem("Scene Manager", nullptr, &S.UI.bScene);
            ImGui::MenuItem("Stat Profiler", nullptr, &S.UI.bStat);
            bool bShadowAtlasDebug = DetailsPanel.IsShadowAtlasDebugWindowOpen();
            if (ImGui::MenuItem("Shadow Atlas Debug", nullptr, &bShadowAtlasDebug))
            {
                DetailsPanel.SetShadowAtlasDebugWindowOpen(bShadowAtlasDebug);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Stat"))
        {
            if (ImGui::MenuItem("Open Stat Profiler"))
            {
                FEditorSettings::Get().UI.bStat = true;
                StatPanel.RequestOpen();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("stat fps"))
            {
                EditorEngine->GetOverlayStatSystem().ShowFPS(true);
                FEditorSettings::Get().UI.bStat = true;
                StatPanel.RequestOpen();
            }
            if (ImGui::MenuItem("stat memory"))
            {
                EditorEngine->GetOverlayStatSystem().ShowMemory(true);
                FEditorSettings::Get().UI.bStat = true;
                StatPanel.RequestOpen();
            }
            if (ImGui::MenuItem("stat lightcull"))
            {
                EditorEngine->GetOverlayStatSystem().ShowLightCull(true);
                FEditorSettings::Get().UI.bStat = true;
                StatPanel.RequestOpen();
            }
            if (ImGui::MenuItem("stat none"))
            {
                EditorEngine->GetOverlayStatSystem().HideAll();
                FEditorSettings::Get().UI.bStat = true;
                StatPanel.RequestOpen();
            }
            ImGui::EndMenu();
        }

        if (ImGui::MenuItem("About"))
        {
            ImGui::OpenPopup("About Editor");
        }

        ImGui::EndMainMenuBar();
    }

    ImGuiViewport* MainViewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(MainViewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("About Editor", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
    {
        ImGui::Dummy(ImVec2(320.0f, 0.0f));
        ImGui::Text("Jungle Editor v0.9.0");
        ImGui::Separator();
        ImGui::Text("Contributors");
        ImGui::BulletText("Alex Kim");
        ImGui::BulletText("Mina Park");
        ImGui::BulletText("Chris Lee");
        ImGui::Spacing();
        ImGui::SetCursorPosX((ImGui::GetWindowSize().x - 120.0f) * 0.5f);
        if (ImGui::Button("Close", ImVec2(120.0f, 0.0f)))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (EditorEngine)
    {
        SCOPE_STAT_CAT("EditorEngine->RenderViewportUI", "5_UI");
        EditorEngine->RenderViewportUI(DeltaTime);
    }

    const FEditorSettings& Settings = FEditorSettings::Get();

    if (!bHideEditorWindows && Settings.UI.bConsole)
    {
        SCOPE_STAT_CAT("ConsolePanel.Render", "5_UI");
        ConsolePanel.Render(DeltaTime);
    }

    if (!bHideEditorWindows && Settings.UI.bControl)
    {
        SCOPE_STAT_CAT("ControlPanel.Render", "5_UI");
        ControlPanel.Render(DeltaTime);
    }

    if (!bHideEditorWindows && Settings.UI.bProperty)
    {
        SCOPE_STAT_CAT("DetailsPanel.Render", "5_UI");
        DetailsPanel.Render(DeltaTime);
    }

    if (!bHideEditorWindows && Settings.UI.bScene)
    {
        SCOPE_STAT_CAT("ScenePanel.Render", "5_UI");
        ScenePanel.Render(DeltaTime);
    }

    if (!bHideEditorWindows && Settings.UI.bStat)
    {
        SCOPE_STAT_CAT("StatPanel.Render", "5_UI");
        StatPanel.Render(DeltaTime);
    }

    if (!bHideEditorWindows && DetailsPanel.IsShadowAtlasDebugWindowOpen())
    {
        SCOPE_STAT_CAT("DetailsPanel.RenderShadowAtlasDebugWindow", "5_UI");
        DetailsPanel.RenderShadowAtlasDebugWindow();
    }

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}


void FEditorMainPanel::Update()
{
    ImGuiIO& IO = ImGui::GetIO();

    bool bWantMouse = IO.WantCaptureMouse;
    bool bWantKeyboard = IO.WantCaptureKeyboard;
    if (EditorEngine && EditorEngine->IsMouseOverViewport())
    {
        bWantMouse = false;
        bWantKeyboard = false;
    }
    InputSystem::Get().GetGuiInputState().bUsingMouse = bWantMouse;
    InputSystem::Get().GetGuiInputState().bUsingKeyboard = bWantKeyboard;

    if (Window)
    {
        HWND hWnd = Window->GetHWND();
        if (IO.WantTextInput)
        {
            ImmAssociateContextEx(hWnd, NULL, IACE_DEFAULT);
        }
        else
        {
            ImmAssociateContext(hWnd, NULL);
        }
    }
}

void FEditorMainPanel::HideEditorWindowsForPIE()
{
    if (bHasSavedUIVisibility)
    {
        bHideEditorWindows = true;
        bShowPanelList = false;
        return;
    }

    FEditorSettings& Settings = FEditorSettings::Get();
    SavedUIVisibility = Settings.UI;
    bSavedShowPanelList = bShowPanelList;
    bHasSavedUIVisibility = true;
    bHideEditorWindows = true;
    bShowPanelList = false;

    Settings.UI.bConsole = false;
    Settings.UI.bControl = false;
    Settings.UI.bProperty = false;
    Settings.UI.bScene = false;
    Settings.UI.bStat = false;
}

void FEditorMainPanel::RestoreEditorWindowsAfterPIE()
{
    if (!bHasSavedUIVisibility)
    {
        bHideEditorWindows = false;
        return;
    }

    FEditorSettings& Settings = FEditorSettings::Get();
    Settings.UI = SavedUIVisibility;
    bShowPanelList = bSavedShowPanelList;
    bHideEditorWindows = false;
    bHasSavedUIVisibility = false;
}
