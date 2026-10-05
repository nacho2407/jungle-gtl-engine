#include "Editor/UI/EditorMainPanel.h"

#include "Editor/EditorEngine.h"

#include "ImGui/imgui.h"

void FEditorMainPanel::OpenRuntimeUIPreviewAsset(const FString& RmlPath)
{
    if (!RmlPath.empty())
    {
        Widgets.RuntimeUIPreviewWidget.OpenPreviewDocument(RmlPath);
    }

    const FString DocumentPath = Widgets.RuntimeUIPreviewWidget.GetPreviewDocumentPath();
    const FEditorTabId TabId = MakeRuntimeUIPreviewTabId();
    const FString TabLabel = MakeRuntimeUIPreviewTabLabel(DocumentPath);
    EditorTabs.OpenOrFocusTab(TabId, TabLabel);
    EditorTabs.SetTabLabel(TabId, TabLabel);
    ActivateEditorTab(TabId);
}

void FEditorMainPanel::QueueRuntimeUIDrawCallback(
    ImDrawList* DrawList,
    const FRuntimeUIRenderContext& Context
)
{
    if (!DrawList || !EditorEngine)
    {
        return;
    }

    FEditorMainPanelPendingRuntimeUIDraw* PendingDraw = new FEditorMainPanelPendingRuntimeUIDraw();
    PendingDraw->Owner = this;
    PendingDraw->Context = Context;
    RuntimeUIDrawState.PendingCallbacks.push_back(PendingDraw);

    DrawList->AddCallback(&FEditorMainPanel::RenderRuntimeUIDrawCallback, PendingDraw);
    DrawList->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
}

void FEditorMainPanel::ClearRuntimeUIDrawCallbacks()
{
    for (FEditorMainPanelPendingRuntimeUIDraw* PendingDraw : RuntimeUIDrawState.PendingCallbacks)
    {
        delete PendingDraw;
    }
    RuntimeUIDrawState.PendingCallbacks.clear();
}

void FEditorMainPanel::RenderRuntimeUIDrawCallback(
    const ImDrawList* ParentList,
    const ImDrawCmd* Cmd
)
{
    (void)ParentList;
    if (!Cmd || !Cmd->UserCallbackData)
    {
        return;
    }

    FEditorMainPanelPendingRuntimeUIDraw* PendingDraw =
        static_cast<FEditorMainPanelPendingRuntimeUIDraw*>(Cmd->UserCallbackData);
    if (!PendingDraw || !PendingDraw->Owner || !PendingDraw->Owner->EditorEngine)
    {
        return;
    }

    PendingDraw->Owner->EditorEngine->GetRmlUiSystem().Render(
        PendingDraw->Context,
        PendingDraw->Owner->EditorEngine->GetRenderer()
    );
}
