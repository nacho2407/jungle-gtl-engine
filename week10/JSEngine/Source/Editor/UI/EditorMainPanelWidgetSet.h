#pragma once

#include "Editor/UI/EditorConsoleWidget.h"
#include "Editor/UI/EditorContentBrowserWidget.h"
#include "Editor/UI/EditorActorSequencerWidget.h"
#include "Editor/UI/EditorControlWidget.h"
#include "Editor/UI/EditorCurveEditorWidget.h"
#include "Editor/UI/EditorMaterialWidget.h"
#include "Editor/UI/EditorPlayStreamWidget.h"
#include "Editor/UI/EditorPropertyWidget.h"
#include "Editor/UI/EditorRuntimeUIPreviewWidget.h"
#include "Editor/UI/EditorSceneWidget.h"
#include "Editor/UI/EditorStatWidget.h"
#include "Editor/UI/EditorToolbarWidget.h"
#include "Editor/UI/EditorViewportOverlayWidget.h"
#include "Editor/UI/EditorViewerWindowWidget.h"

struct FEditorMainPanelWidgetSet
{
	FEditorConsoleWidget ConsoleWidget;
	FEditorContentBrowserWidget ContentBrowserWidget;
	FEditorActorSequencerWidget ActorSequencerWidget;
	FEditorControlWidget ControlWidget;
	FEditorCurveEditorWidget CurveEditorWidget;
	FEditorPropertyWidget PropertyWidget;
	FEditorSceneWidget SceneWidget;
	FEditorMaterialWidget MaterialWidget;
	FEditorViewportOverlayWidget ViewportOverlayWidget;
	FEditorStatWidget StatWidget;
	FEditorToolbarWidget ToolbarWidget;
	FEditorPlayStreamWidget PlayStreamWidget;
	FEditorRuntimeUIPreviewWidget RuntimeUIPreviewWidget;
    TArray<std::unique_ptr<FEditorViewerWindowWidget>> ViewerWindowWidgets;
};
