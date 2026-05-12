#include "Editor/UI/EditorMainPanel.h"

#include "Editor/EditorEngine.h"

#include "ImGui/imgui.h"

FEditorPropertyWidget& FEditorMainPanel::GetPropertyWidget()
{
    return Widgets.PropertyWidget;
}

FEditorMaterialWidget& FEditorMainPanel::GetMaterialWidget()
{
    return Widgets.MaterialWidget;
}

FEditorSceneWidget& FEditorMainPanel::GetSceneWidget()
{
    return Widgets.SceneWidget;
}

FEditorControlWidget& FEditorMainPanel::GetControlWidget()
{
    return Widgets.ControlWidget;
}

void FEditorMainPanel::InitializeEditorWidgets(UEditorEngine* InEditorEngine)
{
    Widgets.ConsoleWidget.Initialize(InEditorEngine);
    Widgets.ContentBrowserWidget.Initialize(InEditorEngine);
    Widgets.ControlWidget.Initialize(InEditorEngine);
    Widgets.CurveEditorWidget.Initialize(InEditorEngine);
    Widgets.MaterialWidget.Initialize(InEditorEngine);
    Widgets.PropertyWidget.Initialize(InEditorEngine);
    Widgets.SceneWidget.Initialize(InEditorEngine);
    Widgets.ViewportOverlayWidget.Initialize(InEditorEngine);
    Widgets.StatWidget.Initialize(InEditorEngine);
    Widgets.PlayStreamWidget.Initialize(InEditorEngine);
    Widgets.ToolbarWidget.Initialize(InEditorEngine);
    Widgets.RuntimeUIPreviewWidget.Initialize(InEditorEngine);
    Widgets.ViewerWindowWidget.Initialize(InEditorEngine);
}

void FEditorMainPanel::OpenCurveAsset(const FString& CurvePath)
{
    Widgets.CurveEditorWidget.OpenCurveAsset(CurvePath);
}

void FEditorMainPanel::BindEditorWidgetCallbacks()
{
    Widgets.RuntimeUIPreviewWidget.SetRmlRenderQueue(
        [this](const FRuntimeUIRenderContext& Context)
        {
            QueueRuntimeUIDrawCallback(ImGui::GetWindowDrawList(), Context);
        });
    Widgets.ToolbarWidget.SetViewportOverlayWidget(&Widgets.ViewportOverlayWidget);
    Widgets.ToolbarWidget.SetSceneWidget(&Widgets.SceneWidget);
    Widgets.ToolbarWidget.SetPlayStreamWidget(&Widgets.PlayStreamWidget);
    Widgets.ToolbarWidget.SetPIEViewportFullscreenCallback([this](bool bEnabled) { SetPIEViewportFullscreenEnabled(bEnabled); });
    Widgets.ToolbarWidget.SetBuildGameCallback([this]() { RequestBuildGame(); });
    Widgets.ToolbarWidget.SetPanelVisibilityRefs(
        &PanelVisibility.bShowConsole,
        &PanelVisibility.bShowControl,
        &PanelVisibility.bShowProperty,
        &PanelVisibility.bShowSceneManager,
        &PanelVisibility.bShowMaterialEditor,
        &PanelVisibility.bShowStatProfiler,
        &PanelVisibility.bShowEditorDebug,
        &PanelVisibility.bShowContentBrowser,
        &PanelVisibility.bShowUndoHistory,
        &PanelVisibility.bShowRuntimeUIPreview,
        &PIEViewportState.bFullscreenEnabled);
}

void FEditorMainPanel::ResetWidgetSelections()
{
    Widgets.PropertyWidget.ResetSelection();
    Widgets.MaterialWidget.ResetSelection();
}
