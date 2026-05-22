#include "Editor/UI/EditorMainPanel.h"

#include "Editor/EditorEngine.h"
#include "Editor/Viewer/EditorViewer.h"
#include "Editor/UI/Viewer/AnimationEditorViewerWidget.h"
#include "Editor/UI/Viewer/ParticleEditorViewerWidget.h"
#include "Editor/UI/Viewer/SkeletalMeshEditorViewerWidget.h"

#include "ImGui/imgui.h"

#include <algorithm>

namespace
{
std::unique_ptr<FEditorViewerWidget> CreateEditorViewerWidget(FEditorViewer* Viewer)
{
	if (!Viewer)
	{
		return std::make_unique<FEditorViewerWidget>();
	}

	switch (Viewer->GetTabKind())
	{
	case EEditorTabKind::AnimSequenceViewer:
		return std::make_unique<FAnimationEditorViewerWidget>();
	case EEditorTabKind::StaticMeshViewer:
		return std::make_unique<FParticleEditorViewerWidget>();
	case EEditorTabKind::SkeletalMeshViewer:
	default:
		return std::make_unique<FSkeletalMeshEditorViewerWidget>();
	}
}
}

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
	Widgets.ActorSequencerWidget.Initialize(InEditorEngine);
	Widgets.AnimGraphWidget.Initialize(InEditorEngine);
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
}

void FEditorMainPanel::OpenCurveAsset(const FString& CurvePath)
{
	Widgets.CurveEditorWidget.OpenCurveAsset(CurvePath);
}

void FEditorMainPanel::OpenAnimGraphAsset(const FString& AnimGraphPath)
{
	if (!AnimGraphPath.empty())
	{
		Widgets.AnimGraphWidget.Open(AnimGraphPath);
	}

	const FEditorTabId TabId = MakeAnimGraphEditorTabId(AnimGraphPath);
	const FString TabLabel = MakeAnimGraphEditorTabLabel(AnimGraphPath);
	EditorTabs.OpenOrFocusTab(TabId, TabLabel);
	EditorTabs.SetTabLabel(TabId, TabLabel);
	ActivateEditorTab(TabId);
}

void FEditorMainPanel::OpenViewer(FEditorViewer* Viewer)
{
	FEditorTabId ViewerTabId;
	if (Viewer)
	{
		ViewerTabId = MakeEditorViewerTabId(Viewer->GetFileName(), Viewer);
		EditorTabs.OpenOrFocusTab(ViewerTabId, MakeEditorViewerTabLabel(Viewer->GetFileName()));
	}

	for (auto& Widget : Widgets.ViewerWindowWidgets)
	{
		if (Widget->GetViewer() == Viewer)
		{
			Widget->SetOpen(true);
			if (EditorTabs.IsTabDetached(ViewerTabId))
			{
				ImGui::SetWindowFocus(Widget->GetWindowName().c_str());
				const TArray<FEditorTabEntry>& Tabs = EditorTabs.GetTabs();
				if (!Tabs.empty())
				{
					ActivateEditorTab(Tabs[0].Id);
				}
			}
			return;
		}
	}

	for (auto* Pending : PendingOpenViewers)
	{
		if (Pending == Viewer)
		{
			return;
		}
	}

	PendingOpenViewers.push_back(Viewer);
}

bool FEditorMainPanel::ChangeViewerTarget(FEditorViewer* Viewer, const FString& NewFileName)
{
	if (!Viewer || NewFileName.empty())
	{
		return false;
	}

	const FEditorTabId OldTabId = MakeEditorViewerTabId(Viewer->GetFileName(), Viewer);
	Viewer->ChangeTarget(NewFileName);

	const FEditorTabId NewTabId = MakeEditorViewerTabId(Viewer->GetFileName(), Viewer);
	const FString NewLabel = MakeEditorViewerTabLabel(Viewer->GetFileName());
	if (!EditorTabs.ReplaceTab(OldTabId, NewTabId, NewLabel))
	{
		EditorTabs.OpenOrFocusTab(NewTabId, NewLabel);
	}
	EditorTabs.SetActiveTab(NewTabId);
	return true;
}

void FEditorMainPanel::FlushOpenViewerWidgets()
{
	auto& V = Widgets.ViewerWindowWidgets;

	for (auto* Viewer : PendingOpenViewers)
	{
		auto WidgetPtr = CreateEditorViewerWidget(Viewer);

		WidgetPtr->Initialize(EditorEngine);
		WidgetPtr->SetViewer(Viewer);
		WidgetPtr->SetOpen(true);

		Widgets.ViewerWindowWidgets.emplace_back(std::move(WidgetPtr));
	}

	PendingOpenViewers.clear();
}

void FEditorMainPanel::CloseViewer(FEditorViewer* Viewer)
{
	if (!Viewer)
	{
		return;
	}

	EditorTabs.CloseTab(MakeEditorViewerTabId(Viewer->GetFileName(), Viewer));
	PendingOpenViewers.erase(std::remove(PendingOpenViewers.begin(), PendingOpenViewers.end(), Viewer), PendingOpenViewers.end());

	// Open false 처리 후 Flush
	for (auto& Widget : Widgets.ViewerWindowWidgets)
		if (Widget->GetViewer() == Viewer)
		{
			Widget->SetOpen(false);
			Widget->SetViewer(nullptr);
			break;
		}
}

void FEditorMainPanel::FlushClosedViewerWidgets()
{
	auto& V = Widgets.ViewerWindowWidgets;
	V.erase(
		std::remove_if(V.begin(), V.end(),
					   [](const std::unique_ptr<FEditorViewerWidget>& W)
					   { return !W || !W->IsOpen() || !W->GetViewer(); }),
		V.end());
}

void FEditorMainPanel::OpenCurveFromActorSequence(
	UCurveFloatAsset* Curve,
	UActorSequenceComponent* SequenceComp,
	const FString& SourceLabel,
	const FString& SourcePath,
	int32 InitialSelectedKeyIndex)
{
	Widgets.CurveEditorWidget.OpenCurveFromActorSequence(
		Curve,
		SequenceComp,
		SourceLabel,
		SourcePath,
		InitialSelectedKeyIndex);
}

void FEditorMainPanel::OpenActorSequencer(UActorSequenceComponent* SequenceComp)
{
	Widgets.ActorSequencerWidget.Open(SequenceComp);
}

void FEditorMainPanel::BindEditorWidgetCallbacks()
{
	Widgets.RuntimeUIPreviewWidget.SetRmlRenderQueue(
		[this](const FRuntimeUIRenderContext& Context)
		{
			QueueRuntimeUIDrawCallback(ImGui::GetWindowDrawList(), Context);
		});
	Widgets.ToolbarWidget.SetViewportOverlayWidget(&Widgets.ViewportOverlayWidget);
	Widgets.ToolbarWidget.SetPlayStreamWidget(&Widgets.PlayStreamWidget);
	Widgets.ToolbarWidget.SetPIEViewportFullscreenCallback([this](bool bEnabled) { SetPIEViewportFullscreenEnabled(bEnabled); });
	Widgets.ToolbarWidget.SetBuildGameCallback([this]() { RequestBuildGame(); });
	Widgets.ToolbarWidget.SetRuntimeUIPreviewOpenCallback([this]() { OpenRuntimeUIPreviewAsset(); });
	Widgets.ToolbarWidget.SetActiveCommandHandlers(
		[this](const FEditorShortcut& Shortcut)
		{
			return ExecuteActiveEditorShortcut(Shortcut);
		},
		[this](EEditorCommandId CommandId)
		{
			return ExecuteActiveEditorCommand(CommandId);
		});
	Widgets.ToolbarWidget.SetActiveMenuRenderer(
		[this]()
		{
			return RenderActiveDocumentMainMenu();
		});
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
		&PanelVisibility.bShowProjectSettings,
		&PanelVisibility.bShowWorldSettings,
		&PIEViewportState.bFullscreenEnabled);
}

void FEditorMainPanel::ResetWidgetSelections()
{
	Widgets.PropertyWidget.ResetSelection();
	Widgets.MaterialWidget.ResetSelection();
	Widgets.ActorSequencerWidget.ResetTarget();
}

