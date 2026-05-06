#include "Editor/UI/EditorMainPanel.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"

#include <algorithm>

void FEditorMainPanel::Render(float DeltaTime)
{
    ClearRuntimeUIDrawCallbacks();
    BeginImGuiFrame();
    TickBuildGameTask();
    HandleContentBrowserShortcut();
    RenderToolbarAndDock(DeltaTime);
    RenderMainViewport(DeltaTime);

    const bool bDrawEditorPanels = !PIEViewportState.bHideEditorWindows;
    RenderEditorPanelWindows(DeltaTime, bDrawEditorPanels);
    RenderBuildGameModal();
    Widgets.ViewportOverlayWidget.RenderFloatingOverlays(DeltaTime);

    const float EffectiveDeltaTime = ResolveEffectiveDeltaTime(DeltaTime);
    UpdateConsoleDrawerAnimation(EffectiveDeltaTime);
    RenderLateFrameOverlays(DeltaTime, EffectiveDeltaTime, bDrawEditorPanels);
    EndImGuiFrame();

    ClearRuntimeUIDrawCallbacks();
}

void FEditorMainPanel::BeginImGuiFrame()
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void FEditorMainPanel::HandleContentBrowserShortcut()
{
    const ImGuiIO& IO = ImGui::GetIO();
    if (!IO.WantTextInput && IO.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Space, false))
    {
        ToggleContentBrowser();
    }
    else
    {
        Widgets.ContentBrowserWidget.SetVisible(PanelVisibility.bShowContentBrowser);
    }
}

void FEditorMainPanel::RenderToolbarAndDock(float DeltaTime)
{
    const bool bContentBrowserVisibleBeforeMenu = PanelVisibility.bShowContentBrowser;
    Widgets.ToolbarWidget.Render(DeltaTime);
    if (!bContentBrowserVisibleBeforeMenu && PanelVisibility.bShowContentBrowser)
    {
        OpenContentBrowser();
    }
    else if (bContentBrowserVisibleBeforeMenu && !PanelVisibility.bShowContentBrowser)
    {
        CloseContentBrowser();
    }
    RenderEditorToolbar();
    RenderDockSpace();
}

void FEditorMainPanel::RenderMainViewport(float DeltaTime)
{
    RenderViewportHostWindow();
    Widgets.ViewportOverlayWidget.RenderViewportFrameOverlays(DeltaTime);
}

void FEditorMainPanel::RenderEditorPanelWindows(float DeltaTime, bool bDrawEditorPanels)
{
    if (bDrawEditorPanels && PanelVisibility.bShowControl)
    {
        Widgets.ControlWidget.Render(DeltaTime);
    }
    if (bDrawEditorPanels && PanelVisibility.bShowMaterialEditor)
    {
        Widgets.MaterialWidget.Render(DeltaTime);
    }
    if (bDrawEditorPanels && PanelVisibility.bShowProperty)
    {
        Widgets.PropertyWidget.Render(DeltaTime);
    }
    if (bDrawEditorPanels && PanelVisibility.bShowSceneManager)
    {
        Widgets.SceneWidget.Render(DeltaTime);
    }
    if (bDrawEditorPanels && PanelVisibility.bShowStatProfiler)
    {
        Widgets.StatWidget.Render(DeltaTime);
    }
    if (bDrawEditorPanels)
    {
        RenderEditorDebugPanel(DeltaTime);
    }
    if (bDrawEditorPanels)
    {
        RenderUndoHistoryPanel(DeltaTime);
    }
    if (bDrawEditorPanels && PanelVisibility.bShowRuntimeUIPreview)
    {
        Widgets.RuntimeUIPreviewWidget.Render(DeltaTime);
    }
    if (bDrawEditorPanels && Widgets.CurveEditorWidget.IsVisible())
    {
        Widgets.CurveEditorWidget.Render(DeltaTime);
    }
    if (bDrawEditorPanels && PanelVisibility.bShowConsole && Widgets.ConsoleWidget.IsFloatingWindowMode())
    {
        Widgets.ConsoleWidget.Render(DeltaTime);
    }
}

float FEditorMainPanel::ResolveEffectiveDeltaTime(float DeltaTime) const
{
    float EffectiveDeltaTime = DeltaTime;
    if (EffectiveDeltaTime <= 0.0f)
    {
        EffectiveDeltaTime = ImGui::GetIO().DeltaTime;
        if (EffectiveDeltaTime <= 0.0f)
        {
            EffectiveDeltaTime = 1.0f / 60.0f;
        }
    }
    return EffectiveDeltaTime;
}

void FEditorMainPanel::UpdateConsoleDrawerAnimation(float EffectiveDeltaTime)
{
    if (!PanelVisibility.bShowConsole)
    {
        ConsoleState.bDrawerVisible = false;
    }

    const float TargetAnim = ConsoleState.bDrawerVisible ? 1.0f : 0.0f;
    constexpr float AnimSpeed = 8.0f;
    if (ConsoleState.DrawerAnim < TargetAnim)
    {
        ConsoleState.DrawerAnim = std::min(1.0f, ConsoleState.DrawerAnim + EffectiveDeltaTime * AnimSpeed);
    }
    else if (ConsoleState.DrawerAnim > TargetAnim)
    {
        ConsoleState.DrawerAnim = std::max(0.0f, ConsoleState.DrawerAnim - EffectiveDeltaTime * AnimSpeed);
    }
}

void FEditorMainPanel::RenderLateFrameOverlays(float DeltaTime, float EffectiveDeltaTime, bool bDrawEditorPanels)
{
    UpdateFooterEventLogs();
    FooterLogSystem.Tick(EffectiveDeltaTime);
    if (bDrawEditorPanels)
    {
        Widgets.ContentBrowserWidget.Render(DeltaTime);
        PanelVisibility.bShowContentBrowser = Widgets.ContentBrowserWidget.IsVisible();
        HandleContentBrowserViewportDrop();
    }
    RenderConsoleDrawer(DeltaTime);
    RenderFooterOverlay(DeltaTime);
}

void FEditorMainPanel::EndImGuiFrame()
{
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}
