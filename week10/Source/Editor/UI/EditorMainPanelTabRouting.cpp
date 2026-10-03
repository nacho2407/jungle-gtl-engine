#include "Editor/UI/EditorMainPanel.h"

#include "Editor/Viewer/EditorViewer.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"

#include <cstring>

namespace
{
	int32 FindImGuiWindowZOrder(const char* WindowName)
	{
		if (!WindowName)
		{
			return 0;
		}

		ImGuiContext* Context = ImGui::GetCurrentContext();
		if (!Context)
		{
			return 0;
		}

		for (int32 Index = 0; Index < Context->Windows.Size; ++Index)
		{
			if (Context->Windows[Index] && std::strcmp(Context->Windows[Index]->Name, WindowName) == 0)
			{
				return Index;
			}
		}
		return 0;
	}

	bool IsWindowInHierarchy(ImGuiWindow* Candidate, ImGuiWindow* Root)
	{
		for (ImGuiWindow* Window = Candidate; Window; Window = Window->ParentWindow)
		{
			if (Window == Root)
			{
				return true;
			}
			if (Window->RootWindow == Root || Window->RootWindowDockTree == Root)
			{
				return true;
			}
		}
		return false;
	}

	bool IsNamedWindowFocusedOrHovered(const char* WindowName, bool bFocusedOnly)
	{
		if (!WindowName)
		{
			return false;
		}

		ImGuiContext* Context = ImGui::GetCurrentContext();
		ImGuiWindow* RootWindow = ImGui::FindWindowByName(WindowName);
		if (!Context || !RootWindow || !RootWindow->WasActive || RootWindow->Hidden)
		{
			return false;
		}

		if (IsWindowInHierarchy(Context->NavWindow, RootWindow))
		{
			return true;
		}
		if (!bFocusedOnly && IsWindowInHierarchy(Context->HoveredWindow, RootWindow))
		{
			return true;
		}
		return false;
	}
}

bool FEditorMainPanel::IsLevelEditorTabActive() const
{
	return EditorTabs.GetActiveTabKind() == EEditorTabKind::LevelEditor;
}

bool FEditorMainPanel::IsLevelEditorViewportVisible() const
{
	return IsLevelEditorTabActive();
}

FEditorViewerWindowWidget* FEditorMainPanel::FindViewerWidgetForTab(const FEditorTabId& TabId) const
{
	if (TabId.Kind != EEditorTabKind::SkeletalMeshViewer &&
		TabId.Kind != EEditorTabKind::StaticMeshViewer)
	{
		return nullptr;
	}

	for (const auto& Widget : Widgets.ViewerWindowWidgets)
	{
		FEditorViewer* Viewer = Widget ? Widget->GetViewer() : nullptr;
		if (Viewer && Viewer->GetFileName() == TabId.PayloadId)
		{
			return Widget.get();
		}
	}

	return nullptr;
}

void FEditorMainPanel::RenderActiveViewerDocument(float DeltaTime)
{
	const FEditorTabEntry* ActiveTab = EditorTabs.GetActiveTab();
	if (!ActiveTab)
	{
		return;
	}

	constexpr ImGuiWindowFlags WindowFlags = 0;
	if (!ImGui::Begin("Viewport", nullptr, WindowFlags))
	{
		ImGui::End();
		return;
	}

	FEditorViewerWindowWidget* ViewerWidget = FindViewerWidgetForTab(ActiveTab->Id);
	if (ActiveTab->bDetached)
	{
		ImGui::TextDisabled("This viewer tab is detached.");
		if (ImGui::Button("Dock Back"))
		{
			RequestDetachEditorTab(ActiveTab->Id, false);
		}
	}
	else if (ViewerWidget)
	{
		ViewerWidget->RenderEmbedded(DeltaTime);
	}
	else
	{
		ImGui::TextDisabled("Viewer tab target is no longer available.");
	}

	ImGui::End();
}

void FEditorMainPanel::RenderRuntimeUIPreviewDocument(float DeltaTime)
{
	const FEditorTabEntry* ActiveTab = EditorTabs.GetActiveTab();
	if (!ActiveTab || ActiveTab->Id.Kind != EEditorTabKind::RuntimeUIPreview)
	{
		return;
	}

	constexpr ImGuiWindowFlags WindowFlags = 0;
	if (!ImGui::Begin("Viewport", nullptr, WindowFlags))
	{
		ImGui::End();
		return;
	}

	Widgets.RuntimeUIPreviewWidget.RenderEmbedded(DeltaTime);
	ImGui::End();
}

void FEditorMainPanel::RequestDockViewer(FEditorViewer* Viewer)
{
	if (!Viewer)
	{
		return;
	}

	RequestDetachEditorTab(MakeEditorViewerTabId(Viewer->GetFileName(), Viewer), false);
}

FEditorTabId FEditorMainPanel::GetInputRoutingTabId() const
{
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		const bool bFocusedOnly = Pass == 0;
		for (const auto& Widget : Widgets.ViewerWindowWidgets)
		{
			FEditorViewer* Viewer = Widget ? Widget->GetViewer() : nullptr;
			if (!Widget || !Viewer || !Widget->IsOpen())
			{
				continue;
			}

			const FEditorTabId ViewerTabId = MakeEditorViewerTabId(Viewer->GetFileName(), Viewer);
			if (!EditorTabs.IsTabDetached(ViewerTabId))
			{
				continue;
			}

			if (IsNamedWindowFocusedOrHovered(Widget->GetWindowName().c_str(), bFocusedOnly))
			{
				return ViewerTabId;
			}
		}
	}

	const FEditorTabEntry* ActiveTab = EditorTabs.GetActiveTab();
	if (ActiveTab && !ActiveTab->bDetached)
	{
		return ActiveTab->Id;
	}

	FEditorTabId NoViewportTabId;
	NoViewportTabId.Kind = EEditorTabKind::LevelEditor;
	NoViewportTabId.PayloadId = "__NoViewportInput";
	return NoViewportTabId;
}

bool FEditorMainPanel::ShouldRouteLevelViewportInput() const
{
	const FEditorTabId RoutingTabId = GetInputRoutingTabId();
	return RoutingTabId.Kind == EEditorTabKind::LevelEditor && RoutingTabId.PayloadId.empty();
}

bool FEditorMainPanel::IsViewerViewportVisible(FEditorViewer* Viewer) const
{
	if (!Viewer)
	{
		return false;
	}

	const FEditorTabId TabId = MakeEditorViewerTabId(Viewer->GetFileName(), Viewer);
	const FEditorTabEntry* ActiveTab = EditorTabs.GetActiveTab();
	if (ActiveTab && ActiveTab->Id.Matches(TabId) && !ActiveTab->bDetached)
	{
		return true;
	}

	if (!EditorTabs.IsTabDetached(TabId))
	{
		return false;
	}

	for (const auto& Widget : Widgets.ViewerWindowWidgets)
	{
		if (Widget && Widget->GetViewer() == Viewer && Widget->IsOpen())
		{
			return true;
		}
	}
	return false;
}

bool FEditorMainPanel::ShouldRouteViewerViewportInput(FEditorViewer* Viewer) const
{
	if (!Viewer || !IsViewerViewportVisible(Viewer))
	{
		return false;
	}

	return true;
}

int32 FEditorMainPanel::GetViewerViewportZOrder(FEditorViewer* Viewer) const
{
	if (!Viewer)
	{
		return 0;
	}

	const FEditorTabId TabId = MakeEditorViewerTabId(Viewer->GetFileName(), Viewer);
	const FEditorTabEntry* ActiveTab = EditorTabs.GetActiveTab();
	if (ActiveTab && ActiveTab->Id.Matches(TabId) && !ActiveTab->bDetached)
	{
		return FindImGuiWindowZOrder("Viewport");
	}

	for (const auto& Widget : Widgets.ViewerWindowWidgets)
	{
		if (Widget && Widget->GetViewer() == Viewer)
		{
			return FindImGuiWindowZOrder(Widget->GetWindowName().c_str());
		}
	}
	return 0;
}
