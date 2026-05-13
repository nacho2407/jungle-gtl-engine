#include "Editor/UI/EditorMainPanel.h"

#include "Editor/EditorEngine.h"
#include "Editor/Viewer/EditorViewer.h"
#include "Editor/Viewport/EditorViewportClient.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"

#include <algorithm>

namespace
{
	struct FEditorTabVisual
	{
		ImVec4 AccentColor;
		const char* Tooltip = "";
	};

	FEditorTabVisual GetTabVisual(EEditorTabKind Kind)
	{
		switch (Kind)
		{
		case EEditorTabKind::LevelEditor:
			return { ImVec4(0.96f, 0.67f, 0.16f, 1.0f), "Level Scene" };
		case EEditorTabKind::StaticMeshViewer:
			return { ImVec4(0.22f, 0.78f, 0.45f, 1.0f), "Static Mesh Viewer" };
		case EEditorTabKind::SkeletalMeshViewer:
			return { ImVec4(0.18f, 0.70f, 0.95f, 1.0f), "Skeletal Mesh Viewer" };
		case EEditorTabKind::MaterialEditor:
			return { ImVec4(0.19f, 0.72f, 0.24f, 1.0f), "Material Editor" };
		case EEditorTabKind::CurveEditor:
			return { ImVec4(0.72f, 0.42f, 0.95f, 1.0f), "Curve Editor" };
		case EEditorTabKind::ActorSequencer:
			return { ImVec4(0.36f, 0.52f, 0.94f, 1.0f), "Actor Sequencer" };
		default:
			return { ImVec4(0.58f, 0.62f, 0.70f, 1.0f), "Editor Tab" };
		}
	}

	const char* GetTabKindDebugName(EEditorTabKind Kind)
	{
		switch (Kind)
		{
		case EEditorTabKind::LevelEditor:
			return "Level";
		case EEditorTabKind::StaticMeshViewer:
			return "StaticMesh";
		case EEditorTabKind::SkeletalMeshViewer:
			return "SkeletalMesh";
		case EEditorTabKind::MaterialEditor:
			return "Material";
		case EEditorTabKind::CurveEditor:
			return "Curve";
		case EEditorTabKind::ActorSequencer:
			return "Sequencer";
		default:
			return "Tab";
		}
	}

	FString MakeLevelEditorTabLabel(UEditorEngine* EditorEngine)
	{
		if (!EditorEngine)
		{
			return "Untitled";
		}

		FString Label = EditorEngine->GetSceneService().GetSceneName();
		if (Label.empty())
		{
			Label = "Untitled";
		}
		if (EditorEngine->GetSceneService().IsDirty())
		{
			Label += "*";
		}
		return Label;
	}

}

void FEditorMainPanel::RenderEditorTabStrip()
{
	ImGuiViewport* MainViewport = ImGui::GetMainViewport();
	if (!MainViewport)
	{
		return;
	}

	constexpr float TabStripHeight = 30.0f;
	const ImVec2 TabStripPos(MainViewport->WorkPos.x, MainViewport->WorkPos.y);
	const ImVec2 TabStripSize(MainViewport->WorkSize.x, TabStripHeight);

	ImGui::SetNextWindowPos(TabStripPos, ImGuiCond_Always);
	ImGui::SetNextWindowSize(TabStripSize, ImGuiCond_Always);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.065f, 0.070f, 0.085f, 1.0f));

	constexpr ImGuiWindowFlags Flags =
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoDocking;

	if (ImGui::Begin("##EditorDocumentTabStrip", nullptr, Flags))
	{
		const FEditorTabEntry* ActiveTab = EditorTabs.GetActiveTab();
		const TArray<FEditorTabEntry>& Tabs = EditorTabs.GetTabs();
		FEditorTabId PendingCloseTabId;
		bool bHasPendingClose = false;
		FEditorTabId PendingDetachTabId;
		bool bHasPendingDetach = false;
		bool bPendingDetachValue = false;
		FEditorTabId PendingMoveFromId;
		FEditorTabId PendingMoveToId;
		bool bHasPendingMove = false;
		static FEditorTabId DraggingDetachTabId;
		static FEditorTabId DraggingTabId;
		static bool bDraggingTab = false;
		static bool bDraggingTabStarted = false;
		ImDrawList* DrawList = ImGui::GetWindowDrawList();

		ImGui::PushID("HomeTab");
		const ImVec2 HomeMin = ImGui::GetCursorScreenPos();
		const ImVec2 HomeMax(HomeMin.x + 32.0f, HomeMin.y + 30.0f);
		ImGui::InvisibleButton("##Home", ImVec2(32.0f, 30.0f));
		const bool bHomeHovered = ImGui::IsItemHovered();
		if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !Tabs.empty())
		{
			ActivateEditorTab(Tabs[0].Id);
		}
		DrawList->AddRectFilled(
			HomeMin,
			HomeMax,
			ImGui::GetColorU32(bHomeHovered ? ImVec4(0.095f, 0.102f, 0.125f, 1.0f) : ImVec4(0.065f, 0.070f, 0.085f, 1.0f)));
		if (IconResources.HomeIcon)
		{
			DrawList->AddImage(
				reinterpret_cast<ImTextureID>(IconResources.HomeIcon),
				ImVec2(HomeMin.x + 5.0f, HomeMin.y + 4.0f),
				ImVec2(HomeMax.x - 5.0f, HomeMax.y - 4.0f));
		}
		else
		{
			const ImU32 HomeColor = ImGui::GetColorU32(ImVec4(0.60f, 0.65f, 0.74f, 1.0f));
			DrawList->AddLine(ImVec2(HomeMin.x + 9.0f, HomeMin.y + 16.0f), ImVec2(HomeMin.x + 16.0f, HomeMin.y + 9.0f), HomeColor, 1.6f);
			DrawList->AddLine(ImVec2(HomeMin.x + 16.0f, HomeMin.y + 9.0f), ImVec2(HomeMin.x + 23.0f, HomeMin.y + 16.0f), HomeColor, 1.6f);
			DrawList->AddRect(ImVec2(HomeMin.x + 11.0f, HomeMin.y + 16.0f), ImVec2(HomeMin.x + 21.0f, HomeMin.y + 23.0f), HomeColor, 1.0f, 0, 1.4f);
		}
		if (bHomeHovered)
		{
			ImGui::SetTooltip("Level");
		}
		ImGui::PopID();
		ImGui::SameLine(0.0f, 0.0f);

		for (int32 Index = 0; Index < static_cast<int32>(Tabs.size()); ++Index)
		{
			const FEditorTabEntry& Tab = Tabs[Index];
			if (Tab.bDetached)
			{
				continue;
			}

			const bool bActive = ActiveTab && ActiveTab->Id.Matches(Tab.Id);

			FString Label = Tab.Label;
			if (Tab.Id.Kind == EEditorTabKind::LevelEditor)
			{
				Label = MakeLevelEditorTabLabel(EditorEngine);
			}
			if (Tab.bDirty)
			{
				Label += "*";
			}
			const FEditorTabVisual Visual = GetTabVisual(Tab.Id.Kind);
			constexpr float TabWidth = 180.0f;
			constexpr float TabHeight = 30.0f;

			ImGui::PushID(Index);
			const ImVec2 TabMin = ImGui::GetCursorScreenPos();
			const ImVec2 TabMax(TabMin.x + TabWidth, TabMin.y + TabHeight);
			ImGui::InvisibleButton("##EditorTab", ImVec2(TabWidth, TabHeight));
			const bool bHovered = ImGui::IsItemHovered();

			const float CloseSize = 16.0f;
			const ImVec2 CloseMin(TabMax.x - CloseSize - 7.0f, TabMin.y + 7.0f);
			const ImVec2 CloseMax(CloseMin.x + CloseSize, CloseMin.y + CloseSize);
			const bool bCloseHovered =
				Tab.bCanClose && ImGui::IsMouseHoveringRect(CloseMin, CloseMax);

			if (Tab.bCanClose && !bCloseHovered && ImGui::IsItemActivated())
			{
				DraggingTabId = Tab.Id;
				bDraggingTab = true;
				bDraggingTabStarted = false;
				DraggingDetachTabId = Tab.Id;
			}
			if (bDraggingTab &&
				Tab.Id.Matches(DraggingTabId) &&
				ImGui::IsMouseDragging(ImGuiMouseButton_Left, 4.0f))
			{
				bDraggingTabStarted = true;
			}

			const ImVec2 MousePos = ImGui::GetIO().MousePos;
			if (bDraggingTabStarted &&
				Index > 0 &&
				!Tab.Id.Matches(DraggingTabId) &&
				MousePos.x >= TabMin.x &&
				MousePos.x <= TabMax.x &&
				MousePos.y >= TabStripPos.y - 8.0f &&
				MousePos.y <= TabStripPos.y + TabStripHeight + 8.0f)
			{
				PendingMoveFromId = DraggingTabId;
				PendingMoveToId = Tab.Id;
				bHasPendingMove = true;
			}

			const ImU32 TabBg = ImGui::GetColorU32(
				bActive
					? ImVec4(0.115f, 0.125f, 0.150f, 1.0f)
					: (bHovered ? ImVec4(0.095f, 0.102f, 0.125f, 1.0f) : ImVec4(0.075f, 0.080f, 0.098f, 1.0f)));
			const ImU32 BorderColor = ImGui::GetColorU32(ImVec4(0.035f, 0.038f, 0.046f, 1.0f));
			DrawList->AddRectFilled(TabMin, TabMax, TabBg, 0.0f);
			DrawList->AddRect(TabMin, TabMax, BorderColor);

			const ImVec2 IconCenter(TabMin.x + 17.0f, TabMin.y + 15.0f);
			DrawList->AddCircleFilled(IconCenter, 5.0f, ImGui::GetColorU32(Visual.AccentColor), 12);
			if (Tab.Id.Kind == EEditorTabKind::LevelEditor)
			{
				DrawList->AddTriangleFilled(
					ImVec2(IconCenter.x, IconCenter.y - 6.0f),
					ImVec2(IconCenter.x - 6.0f, IconCenter.y + 5.0f),
					ImVec2(IconCenter.x + 6.0f, IconCenter.y + 5.0f),
					ImGui::GetColorU32(Visual.AccentColor));
			}

			const bool bTabClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left) && !bCloseHovered;
			if (bTabClicked)
			{
				ActivateEditorTab(Tab.Id);
			}

			const ImU32 TextColor = ImGui::GetColorU32(bActive ? ImVec4(0.88f, 0.91f, 0.96f, 1.0f) : ImVec4(0.58f, 0.62f, 0.70f, 1.0f));
			const ImVec2 TextMin(TabMin.x + 32.0f, TabMin.y + 7.0f);
			const ImVec2 TextMax(
				Tab.bCanClose ? CloseMin.x - 5.0f : TabMax.x - 10.0f,
				TabMin.y + 24.0f);
			ImGui::PushStyleColor(ImGuiCol_Text, TextColor);
			ImGui::RenderTextEllipsis(
				DrawList,
				TextMin,
				TextMax,
				TextMax.x,
				Label.c_str(),
				nullptr,
				nullptr);
			ImGui::PopStyleColor();

			if (Tab.bCanClose)
			{
				const ImU32 CloseColor = ImGui::GetColorU32(
					bCloseHovered ? ImVec4(0.94f, 0.96f, 1.0f, 1.0f) : ImVec4(0.50f, 0.54f, 0.62f, 1.0f));
				if (bCloseHovered)
				{
					DrawList->AddRectFilled(CloseMin, CloseMax, ImGui::GetColorU32(ImVec4(0.23f, 0.25f, 0.31f, 1.0f)), 3.0f);
				}
				DrawList->AddLine(ImVec2(CloseMin.x + 4.0f, CloseMin.y + 4.0f), ImVec2(CloseMax.x - 4.0f, CloseMax.y - 4.0f), CloseColor, 1.5f);
				DrawList->AddLine(ImVec2(CloseMax.x - 4.0f, CloseMin.y + 4.0f), ImVec2(CloseMin.x + 4.0f, CloseMax.y - 4.0f), CloseColor, 1.5f);

				if (bCloseHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
				{
					PendingCloseTabId = Tab.Id;
					bHasPendingClose = true;
				}
			}

			if (bHovered)
			{
				ImGui::SetTooltip(
					"%s\n%s",
					Label.c_str(),
					Visual.Tooltip ? Visual.Tooltip : GetTabKindDebugName(Tab.Id.Kind));
			}

			if (Tab.bCanClose && ImGui::BeginPopupContextItem("##TabContext"))
			{
				if (ImGui::MenuItem(Tab.bDetached ? "Dock Tab" : "Detach Tab"))
				{
					PendingDetachTabId = Tab.Id;
					bHasPendingDetach = true;
					bPendingDetachValue = !Tab.bDetached;
				}
				if (ImGui::MenuItem("Close"))
				{
					PendingCloseTabId = Tab.Id;
					bHasPendingClose = true;
				}
				ImGui::EndPopup();
			}

			if (bActive)
			{
				DrawList->AddLine(
					ImVec2(TabMin.x, TabMax.y - 1.0f),
					ImVec2(TabMax.x, TabMax.y - 1.0f),
					ImGui::GetColorU32(Visual.AccentColor),
					2.0f);
			}

			ImGui::PopID();
			ImGui::SameLine(0.0f, 0.0f);
		}

		if (bHasPendingClose)
		{
			RequestCloseEditorTab(PendingCloseTabId);
		}
		if (bHasPendingDetach)
		{
			RequestDetachEditorTab(PendingDetachTabId, bPendingDetachValue);
		}
		if (bHasPendingMove)
		{
			EditorTabs.MoveTab(PendingMoveFromId, PendingMoveToId);
		}
		else if (bDraggingTab &&
			bDraggingTabStarted &&
			ImGui::IsMouseReleased(ImGuiMouseButton_Left))
		{
			const ImVec2 MousePos = ImGui::GetIO().MousePos;
			const bool bReleasedOutsideStrip =
				MousePos.y < TabStripPos.y - 8.0f ||
				MousePos.y > TabStripPos.y + TabStripHeight + 8.0f;
			if (bReleasedOutsideStrip)
			{
				RequestDetachEditorTab(DraggingDetachTabId, true);
			}
			bDraggingTabStarted = false;
		}
		else if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			bDraggingTab = false;
			bDraggingTabStarted = false;
		}
	}

	ImGui::End();
	ImGui::PopStyleColor();
	ImGui::PopStyleVar(3);
}

void FEditorMainPanel::ActivateEditorTab(const FEditorTabId& TabId)
{
	if (!EditorTabs.SetActiveTab(TabId))
	{
		return;
	}

	if (EditorEngine)
	{
		if (TabId.Kind == EEditorTabKind::LevelEditor)
		{
			FEditorViewportLayout& Layout = EditorEngine->GetViewportLayout();
			const int32 FocusedIndex = Layout.GetLastFocusedViewportIndex();
			if (FEditorViewportClient* Client = Layout.GetViewportClient(FocusedIndex))
			{
				EditorEngine->FocusViewportInput(Client->GetViewport());
			}
		}
		else if (FEditorViewerWindowWidget* ViewerWidget = FindViewerWidgetForTab(TabId))
		{
			if (FEditorViewer* Viewer = ViewerWidget->GetViewer())
			{
				EditorEngine->FocusViewportInput(&Viewer->GetViewport());
			}
		}
	}

	ImGui::SetWindowFocus("Viewport");
}

bool FEditorMainPanel::RequestCloseEditorTab(const FEditorTabId& TabId)
{
	if (TabId.Kind == EEditorTabKind::LevelEditor)
	{
		return false;
	}

	if (TabId.Kind == EEditorTabKind::SkeletalMeshViewer && EditorEngine)
	{
		for (auto& Viewer : EditorEngine->GetViewers())
		{
			if (Viewer && Viewer->GetFileName() == TabId.PayloadId)
			{
				EditorEngine->RemoveViewer(Viewer.get());
				return true;
			}
		}
	}

	return EditorTabs.CloseTab(TabId);
}

void FEditorMainPanel::RequestDetachEditorTab(const FEditorTabId& TabId, bool bDetached)
{
	const FEditorTabEntry* ActiveBefore = EditorTabs.GetActiveTab();
	const bool bWasActive = ActiveBefore && ActiveBefore->Id.Matches(TabId);

	if (!EditorTabs.SetTabDetached(TabId, bDetached))
	{
		return;
	}

	if (FEditorViewerWindowWidget* ViewerWidget = FindViewerWidgetForTab(TabId))
	{
		ViewerWidget->SetOpen(true);
		if (bDetached)
		{
			ImGui::SetWindowFocus(ViewerWidget->GetWindowName().c_str());
			if (bWasActive)
			{
				const TArray<FEditorTabEntry>& Tabs = EditorTabs.GetTabs();
				if (!Tabs.empty())
				{
					ActivateEditorTab(Tabs[0].Id);
				}
			}
		}
		else
		{
			ActivateEditorTab(TabId);
		}
	}
}
