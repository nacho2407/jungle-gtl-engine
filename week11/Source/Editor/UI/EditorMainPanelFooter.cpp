#include "Editor/UI/EditorMainPanel.h"

#include "Editor/EditorEngine.h"
#include "Engine/Core/Paths.h"
#include "Engine/Runtime/Script/ScriptManager.h"
#include "Serialization/SceneSaveManager.h"

#include "ImGui/imgui.h"

#include <algorithm>
#include <filesystem>

namespace
{
FString MakeFooterSceneDisplayPath(const FString& FilePath)
{
    if (FilePath.empty())
    {
        return {};
    }

    std::filesystem::path SceneDir = std::filesystem::path(FSceneSaveManager::GetSceneDirectory()).lexically_normal();
    std::filesystem::path ScenePath = std::filesystem::path(FPaths::ToWide(FilePath)).lexically_normal();
    std::error_code Ec;
    std::filesystem::path RelativePath = std::filesystem::relative(ScenePath, SceneDir.parent_path(), Ec);
    FString Display = Ec ? FPaths::ToUtf8(ScenePath.filename().wstring()) : FPaths::ToUtf8(RelativePath.wstring());
    std::replace(Display.begin(), Display.end(), '/', '\\');
    return Display;
}

} // namespace

void FEditorMainPanel::RenderConsoleDrawer(float DeltaTime)
{
    (void)DeltaTime;
    if (Widgets.ConsoleWidget.IsFloatingWindowMode())
    {
        return;
    }
    constexpr float FooterHeight = 32.0f;
    constexpr float DrawerMaxHeight = 320.0f;
    if (ConsoleState.DrawerAnim <= 0.001f)
    {
        return;
    }

    const ImGuiViewport* MainViewport = ImGui::GetMainViewport();
    const ImVec2 OverlayPos = MainViewport ? MainViewport->WorkPos : ImVec2(0.0f, 0.0f);
    const ImVec2 OverlaySize = MainViewport ? MainViewport->WorkSize : ImGui::GetIO().DisplaySize;
    const float DrawerHeight = DrawerMaxHeight * ConsoleState.DrawerAnim;
    if (DrawerHeight <= 1.0f)
    {
        return;
    }

    ImGui::SetNextWindowPos(
        ImVec2(OverlayPos.x, OverlayPos.y + OverlaySize.y - FooterHeight - DrawerHeight),
        ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(OverlaySize.x, DrawerHeight), ImGuiCond_Always);
    if (MainViewport)
    {
        ImGui::SetNextWindowViewport(MainViewport->ID);
    }

    constexpr ImGuiWindowFlags DrawerFlags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoFocusOnAppearing;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 8.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.09f, 0.11f, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.20f, 0.23f, 0.27f, 1.0f));

    if (ConsoleState.bBringDrawerToFrontNextFrame)
    {
        ImGui::SetNextWindowFocus();
        ConsoleState.bBringDrawerToFrontNextFrame = false;
    }

    if (ImGui::Begin("##EditorConsoleDrawer", nullptr, DrawerFlags))
    {
        Widgets.ConsoleWidget.RenderDrawerToolbar();
        ImGui::Separator();
        Widgets.ConsoleWidget.RenderLogContents(0.0f);
    }
    ImGui::End();

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
}

void FEditorMainPanel::OpenConsoleDrawer(bool bFocusInput)
{
    ConsoleState.bDrawerVisible = true;
    ConsoleState.BacktickCycleState = 2;
    ConsoleState.bBringDrawerToFrontNextFrame = true;
    ConsoleState.bFocusInputNextFrame = bFocusInput;
    CloseContentBrowser();
}

void FEditorMainPanel::CloseConsoleDrawer()
{
    ConsoleState.bDrawerVisible = false;
    ConsoleState.BacktickCycleState = 0;
    ConsoleState.bFocusInputNextFrame = false;
}

void FEditorMainPanel::OpenContentBrowser()
{
    PanelVisibility.bShowContentBrowser = true;
    Widgets.ContentBrowserWidget.SetVisible(true);
    CloseConsoleDrawer();
}

void FEditorMainPanel::CloseContentBrowser()
{
    PanelVisibility.bShowContentBrowser = false;
    Widgets.ContentBrowserWidget.SetVisible(false);
}

void FEditorMainPanel::ToggleContentBrowser()
{
    if (PanelVisibility.bShowContentBrowser)
    {
        CloseContentBrowser();
    }
    else
    {
        OpenContentBrowser();
    }
}

void FEditorMainPanel::PushFooterLog(const FString& Message)
{
    FooterLogSystem.Push(Message, 5.0f);
}

void FEditorMainPanel::RenderFooterOverlay(float DeltaTime)
{
    (void)DeltaTime;
    if (!EditorEngine)
    {
        return;
    }

    const ImGuiViewport* MainViewport = ImGui::GetMainViewport();
    const ImVec2 OverlayPos = MainViewport ? MainViewport->WorkPos : ImVec2(0.0f, 0.0f);
    const ImVec2 OverlaySize = MainViewport ? MainViewport->WorkSize : ImGui::GetIO().DisplaySize;
    constexpr float FooterHeight = 32.0f;

    ImGui::SetNextWindowPos(
        ImVec2(OverlayPos.x, OverlayPos.y + OverlaySize.y - FooterHeight),
        ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(OverlaySize.x, FooterHeight), ImGuiCond_Always);
    if (MainViewport)
    {
        ImGui::SetNextWindowViewport(MainViewport->ID);
    }

    constexpr ImGuiWindowFlags FooterFlags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoFocusOnAppearing;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.11f, 0.14f, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.20f, 0.23f, 0.27f, 1.0f));

    if (ImGui::Begin("##EditorStatusBar", nullptr, FooterFlags))
    {
        const TArray<FString> ActiveLogs = FooterLogSystem.GetActiveMessages();
        if (PanelVisibility.bShowConsole && ImGui::IsKeyPressed(ImGuiKey_GraveAccent, false))
        {
            switch (ConsoleState.BacktickCycleState)
            {
            case 0:
                ConsoleState.BacktickCycleState = 1;
                ConsoleState.bDrawerVisible = false;
                ConsoleState.bFocusInputNextFrame = true;
                CloseContentBrowser();
                break;
            case 1:
                OpenConsoleDrawer(true);
                break;
            default:
                CloseConsoleDrawer();
                ConsoleState.bFocusInputNextFrame = false;
                ConsoleState.bFocusButtonNextFrame = true;
                break;
            }
        }

        const char* ContentLabel = PanelVisibility.bShowContentBrowser
            ? (Widgets.ContentBrowserWidget.IsDrawerMode() ? "Content ▼" : "Content Window")
            : "Content ▲";
        if (ImGui::Button(ContentLabel))
        {
            ToggleContentBrowser();
        }

        ImGui::SameLine();
        const bool bDrawerOpen = ConsoleState.DrawerAnim > 0.5f;
        if (!PanelVisibility.bShowConsole)
        {
            ImGui::TextDisabled("Console Drawer hidden");
        }
        else
        {
            if (ConsoleState.bFocusButtonNextFrame)
            {
                ImGui::SetKeyboardFocusHere();
                ConsoleState.bFocusButtonNextFrame = false;
            }
            const char* ConsoleLabel = Widgets.ConsoleWidget.IsFloatingWindowMode()
                ? "Console Window"
                : (bDrawerOpen ? "Console ▼" : "Console ▲");
            if (ImGui::Button(ConsoleLabel))
            {
                if (Widgets.ConsoleWidget.IsFloatingWindowMode())
                {
                    Widgets.ConsoleWidget.SetPresentationMode(FEditorConsoleWidget::EPresentationMode::Drawer);
                    OpenConsoleDrawer(true);
                }
                else if (!ConsoleState.bDrawerVisible)
                {
                    OpenConsoleDrawer(true);
                }
                else
                {
                    CloseConsoleDrawer();
                    ConsoleState.bFocusButtonNextFrame = true;
                }
            }

            ImGui::SameLine();
            const float InputWidth = OverlaySize.x * (bDrawerOpen ? 0.35f : 0.175f);
            Widgets.ConsoleWidget.RenderInputLine("##FooterConsoleInput", InputWidth, ConsoleState.bFocusInputNextFrame);
            if (ConsoleState.bFocusInputNextFrame)
            {
                ConsoleState.BacktickCycleState = ConsoleState.bDrawerVisible ? 2 : 1;
            }
            ConsoleState.bFocusInputNextFrame = false;
        }

        ImGui::SameLine();
        FString SceneLabel = FString("Level: ") + EditorEngine->GetSceneService().GetCurrentSceneDisplayPath();
        if (EditorEngine->GetEditorState() != EViewportPlayState::Editing)
        {
            const FString ActiveScenePath = EditorEngine->GetCurrentScenePath();
            if (!ActiveScenePath.empty())
            {
                SceneLabel += FString(" | ActiveScene: ") + MakeFooterSceneDisplayPath(ActiveScenePath);
            }
        }
        ImGui::TextDisabled("%s", SceneLabel.c_str());

		ImGui::SameLine();

        {
            const char* Label = "Lua Hot Reload";
            const float ButtonWidth = 136.0f;
            const float TotalWidth = ButtonWidth;
            const float RightX = ImGui::GetWindowContentRegionMax().x;
            const float ButtonStartX = RightX - TotalWidth;

            if (!ActiveLogs.empty())
            {
                const FString& LatestFooterLog = ActiveLogs.back();
                const float LogWidth = ImGui::CalcTextSize(LatestFooterLog.c_str()).x;
                const float MinLogX = ImGui::GetCursorPosX() + 16.0f;
                const float LogRightPadding = 18.0f;
                const float LogX = std::max(MinLogX, ButtonStartX - LogRightPadding - LogWidth);
                ImGui::SetCursorPosX(LogX);
                ImGui::TextUnformatted(LatestFooterLog.c_str());
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("%s", LatestFooterLog.c_str());
                }
                ImGui::SameLine();
            }

            ImGui::SetCursorPosX(ButtonStartX);
            if (ImGui::Button(Label, ImVec2(ButtonWidth, 0.0f)))
            {
                FScriptManager::Get().HotReloadScripts();
                PushFooterLog("Lua scripts hot reloaded");
            }
        }
    }

    ImGui::End();

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
}

void FEditorMainPanel::UpdateFooterEventLogs()
{
    if (!EditorEngine)
    {
        return;
    }

    const EViewportPlayState CurrentState = EditorEngine->GetEditorState();
    const bool bPIEPlaying = CurrentState != EViewportPlayState::Editing;
    if (!FooterEventState.bInitialized)
    {
        FooterEventState.bInitialized = true;
        FooterEventState.bPrevPIEPlaying = bPIEPlaying;
        FooterEventState.PrevEditorState = CurrentState;
        return;
    }

    if (!FooterEventState.bPrevPIEPlaying && bPIEPlaying)
    {
        FooterLogSystem.Push("PIE started");
    }
    else if (FooterEventState.bPrevPIEPlaying && !bPIEPlaying)
    {
        FooterLogSystem.Push("PIE ended");
    }
    else if (FooterEventState.PrevEditorState != CurrentState)
    {
        switch (CurrentState)
        {
        case EViewportPlayState::Paused:
            FooterLogSystem.Push("PIE paused");
            break;
        case EViewportPlayState::Playing:
            FooterLogSystem.Push("PIE resumed");
            break;
        default:
            break;
        }
    }

    FooterEventState.bPrevPIEPlaying = bPIEPlaying;
    FooterEventState.PrevEditorState = CurrentState;
}
