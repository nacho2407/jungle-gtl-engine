#include "Editor/UI/EditorMainPanel.h"

#include "Editor/Settings/ProjectSettings.h"
#include "Engine/Core/Paths.h"
#include "Engine/Runtime/WindowsWindow.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"

#include "Render/Renderer/Renderer.h"

#include <filesystem>

void FEditorMainPanel::Create(FWindowsWindow* InWindow, FRenderer& InRenderer, UEditorEngine* InEditorEngine)
{
    InitializeImGuiContext();

    Window = InWindow;
    EditorEngine = InEditorEngine;

    LoadProjectSettings();
    InitializeImGuiBackend(InWindow, InRenderer);
    InitializeEditorWidgets(InEditorEngine);
    BindEditorWidgetCallbacks();
}

void FEditorMainPanel::InitializeImGuiContext()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& IO = ImGui::GetIO();
    IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    IO.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    IO.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    IO.MouseDrawCursor = false;
    ConfigureImGuiStyle();
}

void FEditorMainPanel::LoadProjectSettings()
{
    const FString ProjectSettingsPath = FProjectSettings::GetDefaultSettingsPath();
    const bool bHadProjectSettings = std::filesystem::exists(std::filesystem::path(FPaths::ToWide(ProjectSettingsPath)));
    FProjectSettings::Get().LoadFromFile(ProjectSettingsPath);
    if (!bHadProjectSettings)
    {
        FProjectSettings::Get().SaveToFile(ProjectSettingsPath);
    }
}

void FEditorMainPanel::InitializeImGuiBackend(FWindowsWindow* InWindow, FRenderer& InRenderer)
{
    LoadEditorFonts();
    ImGui_ImplWin32_Init((void*)InWindow->GetHWND());
    ImGui_ImplDX11_Init(InRenderer.GetFD3DDevice().GetDevice(), InRenderer.GetFD3DDevice().GetDeviceContext());
    LoadViewportToolIcons(InRenderer.GetFD3DDevice().GetDevice());
}

void FEditorMainPanel::Release()
{
    ReleaseViewportToolIcons();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}
