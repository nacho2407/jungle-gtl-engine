#include "Editor/UI/EditorMainPanel.h"

#include "Editor/EditorEngine.h"
#include "Engine/Input/InputSystem.h"

#include <algorithm>

void FEditorMainPanel::RequestPIEViewportInputFocus()
{
    PIEViewportState.PendingInputFocusFrames = 3;
    ConsoleState.bDrawerVisible = false;
    ConsoleState.bFocusInputNextFrame = false;
    ConsoleState.bFocusButtonNextFrame = false;
}

void FEditorMainPanel::HideEditorWindowsForPIE()
{
    if (!PIEViewportState.bHasSavedPanelVisibility)
    {
        PIEViewportState.SavedPanelVisibility.bShowConsole = PanelVisibility.bShowConsole;
        PIEViewportState.SavedPanelVisibility.bShowControl = PanelVisibility.bShowControl;
        PIEViewportState.SavedPanelVisibility.bShowProperty = PanelVisibility.bShowProperty;
        PIEViewportState.SavedPanelVisibility.bShowSceneManager = PanelVisibility.bShowSceneManager;
        PIEViewportState.SavedPanelVisibility.bShowMaterialEditor = PanelVisibility.bShowMaterialEditor;
        PIEViewportState.SavedPanelVisibility.bShowStatProfiler = PanelVisibility.bShowStatProfiler;
        PIEViewportState.SavedPanelVisibility.bShowPlayStream = PanelVisibility.bShowPlayStream;
        PIEViewportState.SavedPanelVisibility.bShowEditorDebug = PanelVisibility.bShowEditorDebug;
        PIEViewportState.SavedPanelVisibility.bShowContentBrowser = PanelVisibility.bShowContentBrowser;
        PIEViewportState.SavedPanelVisibility.bConsoleDrawerVisible = ConsoleState.bDrawerVisible;
        PIEViewportState.SavedPanelVisibility.bViewportSettingsVisible =
            Widgets.ViewportOverlayWidget.IsViewportSettingsVisible();
        PIEViewportState.SavedPanelVisibility.bGroupedStatOverlayVisible =
            Widgets.ViewportOverlayWidget.IsGroupedStatOverlayVisible();
        PIEViewportState.bHasSavedPanelVisibility = true;
    }

    PIEViewportState.bHideEditorWindows = true;
    if (PIEViewportState.bFullscreenEnabled)
    {
        PanelVisibility.bShowConsole = false;
        PanelVisibility.bShowControl = false;
        PanelVisibility.bShowProperty = false;
        PanelVisibility.bShowSceneManager = false;
        PanelVisibility.bShowMaterialEditor = false;
        PanelVisibility.bShowStatProfiler = false;
        PanelVisibility.bShowPlayStream = false;
        PanelVisibility.bShowEditorDebug = false;
        PanelVisibility.bShowContentBrowser = false;
        Widgets.ContentBrowserWidget.SetVisible(false);
        ConsoleState.bDrawerVisible = false;
        Widgets.ViewportOverlayWidget.SetViewportSettingsVisible(false);
        Widgets.ViewportOverlayWidget.SetGroupedStatOverlayVisible(false);
        ApplyPIEViewportFullscreen();
    }
    else
    {
        PIEViewportState.bHideEditorWindows = false;
    }
}

void FEditorMainPanel::RestoreEditorWindowsAfterPIE()
{
    PIEViewportState.bHideEditorWindows = false;
    if (!PIEViewportState.bHasSavedPanelVisibility)
    {
        RestorePIEViewportLayout();
        return;
    }

    PanelVisibility.bShowConsole = PIEViewportState.SavedPanelVisibility.bShowConsole;
    PanelVisibility.bShowControl = PIEViewportState.SavedPanelVisibility.bShowControl;
    PanelVisibility.bShowProperty = PIEViewportState.SavedPanelVisibility.bShowProperty;
    PanelVisibility.bShowSceneManager = PIEViewportState.SavedPanelVisibility.bShowSceneManager;
    PanelVisibility.bShowMaterialEditor = PIEViewportState.SavedPanelVisibility.bShowMaterialEditor;
    PanelVisibility.bShowStatProfiler = PIEViewportState.SavedPanelVisibility.bShowStatProfiler;
    PanelVisibility.bShowPlayStream = PIEViewportState.SavedPanelVisibility.bShowPlayStream;
    PanelVisibility.bShowEditorDebug = PIEViewportState.SavedPanelVisibility.bShowEditorDebug;
    PanelVisibility.bShowContentBrowser = PIEViewportState.SavedPanelVisibility.bShowContentBrowser;
    Widgets.ContentBrowserWidget.SetVisible(PanelVisibility.bShowContentBrowser);
    ConsoleState.bDrawerVisible = PIEViewportState.SavedPanelVisibility.bConsoleDrawerVisible;
    Widgets.ViewportOverlayWidget.SetViewportSettingsVisible(
        PIEViewportState.SavedPanelVisibility.bViewportSettingsVisible
    );
    Widgets.ViewportOverlayWidget.SetGroupedStatOverlayVisible(
        PIEViewportState.SavedPanelVisibility.bGroupedStatOverlayVisible
    );
    PIEViewportState.bHasSavedPanelVisibility = false;
    RestorePIEViewportLayout();
}

bool FEditorMainPanel::IsPIEViewportFullscreenEnabled() const
{
    return PIEViewportState.bFullscreenEnabled;
}

void FEditorMainPanel::SetPIEViewportFullscreenEnabled(bool bEnabled)
{
    if (PIEViewportState.bFullscreenEnabled == bEnabled)
    {
        return;
    }

    PIEViewportState.bFullscreenEnabled = bEnabled;
    if (!EditorEngine || EditorEngine->GetEditorState() == EViewportPlayState::Editing)
    {
        return;
    }

    if (PIEViewportState.bFullscreenEnabled)
    {
        if (!PIEViewportState.bHasSavedPanelVisibility)
        {
            PIEViewportState.SavedPanelVisibility.bShowConsole = PanelVisibility.bShowConsole;
            PIEViewportState.SavedPanelVisibility.bShowControl = PanelVisibility.bShowControl;
            PIEViewportState.SavedPanelVisibility.bShowProperty = PanelVisibility.bShowProperty;
            PIEViewportState.SavedPanelVisibility.bShowSceneManager = PanelVisibility.bShowSceneManager;
            PIEViewportState.SavedPanelVisibility.bShowMaterialEditor = PanelVisibility.bShowMaterialEditor;
            PIEViewportState.SavedPanelVisibility.bShowStatProfiler = PanelVisibility.bShowStatProfiler;
            PIEViewportState.SavedPanelVisibility.bShowPlayStream = PanelVisibility.bShowPlayStream;
            PIEViewportState.SavedPanelVisibility.bShowEditorDebug = PanelVisibility.bShowEditorDebug;
            PIEViewportState.SavedPanelVisibility.bShowContentBrowser = PanelVisibility.bShowContentBrowser;
            PIEViewportState.SavedPanelVisibility.bConsoleDrawerVisible = ConsoleState.bDrawerVisible;
            PIEViewportState.SavedPanelVisibility.bViewportSettingsVisible =
                Widgets.ViewportOverlayWidget.IsViewportSettingsVisible();
            PIEViewportState.SavedPanelVisibility.bGroupedStatOverlayVisible =
                Widgets.ViewportOverlayWidget.IsGroupedStatOverlayVisible();
            PIEViewportState.bHasSavedPanelVisibility = true;
        }

        PIEViewportState.bHideEditorWindows = true;
        PanelVisibility.bShowConsole = false;
        PanelVisibility.bShowControl = false;
        PanelVisibility.bShowProperty = false;
        PanelVisibility.bShowSceneManager = false;
        PanelVisibility.bShowMaterialEditor = false;
        PanelVisibility.bShowStatProfiler = false;
        PanelVisibility.bShowPlayStream = false;
        PanelVisibility.bShowEditorDebug = false;
        PanelVisibility.bShowContentBrowser = false;
        Widgets.ContentBrowserWidget.SetVisible(false);
        ConsoleState.bDrawerVisible = false;
        Widgets.ViewportOverlayWidget.SetViewportSettingsVisible(false);
        Widgets.ViewportOverlayWidget.SetGroupedStatOverlayVisible(false);
        ApplyPIEViewportFullscreen();
    }
    else
    {
        PIEViewportState.bHideEditorWindows = false;
        if (PIEViewportState.bHasSavedPanelVisibility)
        {
            PanelVisibility.bShowConsole = PIEViewportState.SavedPanelVisibility.bShowConsole;
            PanelVisibility.bShowControl = PIEViewportState.SavedPanelVisibility.bShowControl;
            PanelVisibility.bShowProperty = PIEViewportState.SavedPanelVisibility.bShowProperty;
            PanelVisibility.bShowSceneManager = PIEViewportState.SavedPanelVisibility.bShowSceneManager;
            PanelVisibility.bShowMaterialEditor = PIEViewportState.SavedPanelVisibility.bShowMaterialEditor;
            PanelVisibility.bShowStatProfiler = PIEViewportState.SavedPanelVisibility.bShowStatProfiler;
            PanelVisibility.bShowPlayStream = PIEViewportState.SavedPanelVisibility.bShowPlayStream;
            PanelVisibility.bShowEditorDebug = PIEViewportState.SavedPanelVisibility.bShowEditorDebug;
            PanelVisibility.bShowContentBrowser = PIEViewportState.SavedPanelVisibility.bShowContentBrowser;
            Widgets.ContentBrowserWidget.SetVisible(PanelVisibility.bShowContentBrowser);
            ConsoleState.bDrawerVisible = PIEViewportState.SavedPanelVisibility.bConsoleDrawerVisible;
            Widgets.ViewportOverlayWidget.SetViewportSettingsVisible(
                PIEViewportState.SavedPanelVisibility.bViewportSettingsVisible
            );
            Widgets.ViewportOverlayWidget.SetGroupedStatOverlayVisible(
                PIEViewportState.SavedPanelVisibility.bGroupedStatOverlayVisible
            );
            PIEViewportState.bHasSavedPanelVisibility = false;
        }
        RestorePIEViewportLayout();
    }
}

void FEditorMainPanel::ApplyPIEViewportFullscreen()
{
    if (!EditorEngine || !PIEViewportState.bFullscreenEnabled)
    {
        return;
    }

    FEditorViewportLayout& Layout = EditorEngine->GetViewportLayout();
    const int32 FocusedIndex = Layout.GetLastFocusedViewportIndex();

    if (!PIEViewportState.SavedLayout.bValid)
    {
        PIEViewportState.SavedLayout.bValid = true;
        PIEViewportState.SavedLayout.LayoutMode = Layout.GetLayoutMode();
        PIEViewportState.SavedLayout.SingleViewportIndex = Layout.GetSingleViewportIndex();
        PIEViewportState.SavedLayout.LastFocusedViewportIndex = FocusedIndex;
    }

    Layout.SetLayoutModeAnimated(EEditorViewportLayoutMode::OnePane, FocusedIndex);
}

void FEditorMainPanel::RestorePIEViewportLayout()
{
    if (!EditorEngine || !PIEViewportState.SavedLayout.bValid)
    {
        return;
    }

    FEditorViewportLayout& Layout = EditorEngine->GetViewportLayout();
    Layout.SetLayoutMode(
        PIEViewportState.SavedLayout.LayoutMode,
        PIEViewportState.SavedLayout.SingleViewportIndex
    );
    Layout.SetLastFocusedViewportIndex(PIEViewportState.SavedLayout.LastFocusedViewportIndex);
    PIEViewportState.SavedLayout = FEditorMainPanelPIEViewportLayoutSnapshot{};
}

FViewportRect FEditorMainPanel::GetPIEFixedAspectViewportRect(
    const FViewportRect& SourceRect
) const
{
    if (SourceRect.Width <= 0 || SourceRect.Height <= 0 ||
        PIEViewportState.UILayoutWidth <= 0 || PIEViewportState.UILayoutHeight <= 0)
    {
        return SourceRect;
    }

    const float TargetAspect =
        static_cast<float>(PIEViewportState.UILayoutWidth) / static_cast<float>(PIEViewportState.UILayoutHeight);
    const float SourceAspect =
        static_cast<float>(SourceRect.Width) / static_cast<float>(SourceRect.Height);

    int32 Width = SourceRect.Width;
    int32 Height = SourceRect.Height;
    if (SourceAspect > TargetAspect)
    {
        Width = std::max(static_cast<int32>(static_cast<float>(SourceRect.Height) * TargetAspect), 1);
    }
    else
    {
        Height = std::max(static_cast<int32>(static_cast<float>(SourceRect.Width) / TargetAspect), 1);
    }

    const int32 X = SourceRect.X + (SourceRect.Width - Width) / 2;
    const int32 Y = SourceRect.Y + (SourceRect.Height - Height) / 2;
    return FViewportRect(X, Y, Width, Height);
}

void FEditorMainPanel::ApplyPIEFixedAspectViewportRect()
{
    if (!EditorEngine || !PIEViewportState.bUseFixedUILayout ||
        EditorEngine->GetEditorState() == EViewportPlayState::Editing)
    {
        return;
    }

    FEditorViewportLayout& Layout = EditorEngine->GetViewportLayout();
    const int32 ActivePIEViewportIndex = EditorEngine->GetPIESession().GetActiveViewportIndex();
    const int32 PIEViewportIndex = (ActivePIEViewportIndex >= 0)
        ? ActivePIEViewportIndex
        : Layout.GetLastFocusedViewportIndex();
    if (PIEViewportIndex < 0 || PIEViewportIndex >= FEditorViewportLayout::MaxViewports)
    {
        return;
    }

    FEditorViewportClient* Client = Layout.GetViewportClient(PIEViewportIndex);
    if (!Client || !Client->IsPIEPossessed())
    {
        return;
    }

    FSceneViewport& SceneViewport = Layout.GetSceneViewport(PIEViewportIndex);
    const FViewportRect SourceRect = SceneViewport.GetRect();
    const FViewportRect FixedRect = GetPIEFixedAspectViewportRect(SourceRect);
    if (FixedRect.X == SourceRect.X && FixedRect.Y == SourceRect.Y &&
        FixedRect.Width == SourceRect.Width && FixedRect.Height == SourceRect.Height)
    {
        return;
    }

    SceneViewport.SetRect(FixedRect);
    Client->SetViewportSize(static_cast<float>(FixedRect.Width), static_cast<float>(FixedRect.Height));
}

void FEditorMainPanel::RenderRuntimeUIForPIEViewport(
    const FViewportRect& ViewportRect,
    float DeltaTime
)
{
    if (!EditorEngine || ViewportRect.Width <= 0 || ViewportRect.Height <= 0)
    {
        return;
    }

    FEditorViewportLayout& Layout = EditorEngine->GetViewportLayout();
    const int32 ActivePIEViewportIndex = EditorEngine->GetPIESession().GetActiveViewportIndex();
    const int32 PIEViewportIndex = (ActivePIEViewportIndex >= 0)
        ? ActivePIEViewportIndex
        : Layout.GetLastFocusedViewportIndex();
    FEditorViewportClient* Client = Layout.GetViewportClient(PIEViewportIndex);
    if (!Client || !Client->IsPIEPossessed())
    {
        return;
    }

    const FViewportRect RuntimeUIRect = PIEViewportState.bUseFixedUILayout
        ? GetPIEFixedAspectViewportRect(ViewportRect)
        : ViewportRect;
    const int32 LayoutWidth =
        PIEViewportState.bUseFixedUILayout ? std::max(PIEViewportState.UILayoutWidth, 1) : RuntimeUIRect.Width;
    const int32 LayoutHeight =
        PIEViewportState.bUseFixedUILayout ? std::max(PIEViewportState.UILayoutHeight, 1) : RuntimeUIRect.Height;

    FRuntimeUIRenderContext Context;
    Context.RenderMode = ERuntimeUIRenderMode::PIE;
    Context.ViewportMin =
        FRuntimeUIVector2(static_cast<float>(RuntimeUIRect.X), static_cast<float>(RuntimeUIRect.Y));
    Context.ViewportSize =
        FRuntimeUIVector2(static_cast<float>(RuntimeUIRect.Width), static_cast<float>(RuntimeUIRect.Height));
    Context.LayoutSize =
        FRuntimeUIVector2(static_cast<float>(LayoutWidth), static_cast<float>(LayoutHeight));
    Context.DeltaTime = DeltaTime;

    QueueRuntimeUIDrawCallback(ImGui::GetWindowDrawList(), Context);

    const FGuiInputState& GuiState = InputSystem::Get().GetGuiInputState();
    const bool bAllowRuntimeUIInput =
        EditorEngine->BuildRuntimeInputPermissions(GuiState).bAllowRuntimeUIInput;
    if (EditorEngine->GetRmlUiSystem().PumpViewportInput(
        InputSystem::Get(),
        EditorEngine->GetWindow(),
        bAllowRuntimeUIInput,
        RuntimeUIRect,
        LayoutWidth,
        LayoutHeight
    ))
    {
        InputSystem::Get().SetGuiMouseCapture(true);
        InputSystem::Get().SetGuiViewportMouseBlock(true);
    }
}
