#include "Editor/Input/EditorViewportTools.h"

#include "Editor/Viewport/EditorViewportClient.h"

bool FEditorViewportCommandTool::HandleInput(FEditorViewportClient& Owner, const FViewportInputContext& Context)
{
    return Owner.HandleCommandInput(Context);
}

bool FEditorViewportGizmoTool::HandleInput(FEditorViewportClient& Owner, const FViewportInputContext& Context)
{
    return Owner.HandleGizmoInput(Context);
}

bool FEditorViewportSelectionTool::HandleInput(FEditorViewportClient& Owner, const FViewportInputContext& Context)
{
    return Owner.HandleSelectionInput(Context);
}

bool FEditorViewportNavigationTool::HandleInput(FEditorViewportClient& Owner, const FViewportInputContext& Context)
{
    return Owner.HandleNavigationInput(Context);
}
