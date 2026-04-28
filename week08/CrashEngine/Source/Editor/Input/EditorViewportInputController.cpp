#include "EditorViewportInputController.h"

#include "EditorGizmoTool.h"
#include "EditorNavigationTool.h"
#include "EditorSelectionTool.h"
#include "EditorViewportCommandTool.h"
#include "EditorViewportTool.h"

FEditorViewportInputController::FEditorViewportInputController(FEditorViewportClient* InOwner)
    : Owner(InOwner)
{
	// 순서 중요!!!
    Tools.emplace_back(std::make_unique<FEditorViewportCommandTool>(Owner, this));
    Tools.emplace_back(std::make_unique<FEditorGizmoTool>(Owner, this));
    Tools.emplace_back(std::make_unique<FEditorSelectionTool>(Owner, this));
    Tools.emplace_back(std::make_unique<FEditorNavigationTool>(Owner, this));
}

bool FEditorViewportInputController::HandleInput(float DeltaTime)
{
    for (const std::unique_ptr<FEditorViewportTool>& Tool : Tools)
    {
        if (Tool && Tool->HandleInput(DeltaTime))
        {
            return true;
        }
    }

    return false;
}
