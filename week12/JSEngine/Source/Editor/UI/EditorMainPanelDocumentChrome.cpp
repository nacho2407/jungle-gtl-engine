#include "Editor/UI/EditorMainPanel.h"

#include "Editor/UI/EditorChromeConstants.h"
#include "Editor/UI/EditorMainPanelViewportToolbarHelpers.h"
#include "Editor/Viewer/EditorViewer.h"
#include "Editor/Viewer/SkeletalAssetEditorViewer.h"
#include "Editor/Viewport/EditorViewportClient.h"

#include "ImGui/imgui.h"

#include <cstdio>

namespace
{
	FSkeletalAssetEditorViewer* AsSkeletalAssetViewer(FEditorViewer* Viewer)
	{
		if (!Viewer)
		{
			return nullptr;
		}

		const EEditorTabKind TabKind = Viewer->GetTabKind();
		if (TabKind == EEditorTabKind::SkeletalMeshViewer || TabKind == EEditorTabKind::AnimSequenceViewer)
		{
			return static_cast<FSkeletalAssetEditorViewer*>(Viewer);
		}
		return nullptr;
	}
}

void FEditorMainPanel::RenderActiveDocumentToolbar()
{
	ImGuiViewport* MainViewport = ImGui::GetMainViewport();
	if (!MainViewport)
	{
		return;
	}

	constexpr float TabStripHeight = FEditorChromeMetrics::TabStripHeight;
	constexpr float ToolbarHeight = FEditorChromeMetrics::DocumentToolbarHeight;
	const ImVec2 ToolbarPos(
		MainViewport->WorkPos.x,
		MainViewport->WorkPos.y + FEditorChromeMetrics::ApplicationTitleBarHeight + TabStripHeight);
	const ImVec2 ToolbarSize(MainViewport->WorkSize.x, ToolbarHeight);

	ImGui::SetNextWindowPos(ToolbarPos, ImGuiCond_Always);
	ImGui::SetNextWindowSize(ToolbarSize, ImGuiCond_Always);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 6.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.11f, 0.14f, 0.98f));

	constexpr ImGuiWindowFlags Flags =
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoDocking;

	if (ImGui::Begin("##EditorDocumentToolbar", nullptr, Flags))
	{
		const FEditorTabEntry* ActiveTab = EditorTabs.GetActiveTab();
		FEditorViewerWidget* ViewerWidget = ActiveTab ? FindViewerWidgetForTab(ActiveTab->Id) : nullptr;
		FEditorViewer* Viewer = ViewerWidget ? ViewerWidget->GetViewer() : nullptr;

		if (Viewer)
		{
			if (Viewer->GetTabKind() != EEditorTabKind::ParticleViewer)
			{
				ImGui::TextDisabled(Viewer->GetViewerLabel());
				ImGui::SameLine();
				ImGui::TextUnformatted(Viewer->GetFileName().c_str());
			}
		}
		else if (ActiveTab && ActiveTab->Id.Kind == EEditorTabKind::RuntimeUIPreview)
		{
			ImGui::TextDisabled("Runtime UI Preview");
			ImGui::SameLine();
			ImGui::TextUnformatted(Widgets.RuntimeUIPreviewWidget.GetPreviewDocumentPath().c_str());
		}
		else if (ActiveTab && ActiveTab->Id.Kind == EEditorTabKind::AnimGraphEditor)
		{
			ImGui::TextDisabled("Anim Graph");
			ImGui::SameLine();
			ImGui::TextUnformatted(Widgets.AnimGraphWidget.GetEditingPath().c_str());
		}
		else
		{
			ImGui::TextDisabled("No active document");
		}
	}

	ImGui::End();
	ImGui::PopStyleColor();
	ImGui::PopStyleVar(2);
}

void FEditorMainPanel::RenderViewerToolbarControls(FEditorViewer* Viewer)
{
	if (!Viewer)
	{
		ImGui::TextDisabled("No viewer");
		return;
	}

	FEditorViewportClient* Client = &Viewer->GetClient();
	FEditorViewportState* ViewportState = Client ? Client->GetViewportState() : nullptr;
	FSkeletalAssetEditorViewer* SkeletalViewer = AsSkeletalAssetViewer(Viewer);
	FSkeletalMeshViewerShowFlags* ShowFlags = SkeletalViewer ? &SkeletalViewer->GetClient().GetShowFlags() : nullptr;

	RenderViewerTransformToolbarControls(Viewer);

	ImGui::PushID(Viewer);
	ImGui::SameLine(0.0f, 10.0f);
	char TypeButtonLabel[64];
	snprintf(
		TypeButtonLabel,
		sizeof(TypeButtonLabel),
		"%s ▼",
		FEditorMainPanelViewportToolbarHelpers::GetViewportTypeName(Client->GetViewportType()));
	if (DrawViewportTextButton("##ViewerViewportTypeShared", TypeButtonLabel))
	{
		ImGui::OpenPopup("##ViewerViewportTypePopupShared");
	}
	if (ImGui::BeginPopup("##ViewerViewportTypePopupShared"))
	{
		static constexpr EEditorViewportType Types[] = {
			EVT_Perspective,
			EVT_OrthoTop,
			EVT_OrthoBottom,
			EVT_OrthoFront,
			EVT_OrthoBack,
			EVT_OrthoLeft,
			EVT_OrthoRight,
		};
		for (EEditorViewportType Type : Types)
		{
			if (ImGui::MenuItem(
				FEditorMainPanelViewportToolbarHelpers::GetViewportTypeName(Type),
				nullptr,
				Client->GetViewportType() == Type))
			{
				Client->SetViewportType(Type);
				Client->ApplyCameraMode();
			}
		}
		ImGui::EndPopup();
	}

	ImGui::SameLine();
	char CameraButtonLabel[48];
	snprintf(
		CameraButtonLabel,
		sizeof(CameraButtonLabel),
		"Cam %.1fx ▼",
		FEditorMainPanelViewportToolbarHelpers::GetCameraSpeedMultiplier(Client));
	if (DrawViewportTextButton("##ViewerCameraSpeedShared", CameraButtonLabel))
	{
		ImGui::OpenPopup("##ViewerCameraSpeedPopupShared");
	}
	if (ImGui::BeginPopup("##ViewerCameraSpeedPopupShared"))
	{
		float SpeedMultiplier = FEditorMainPanelViewportToolbarHelpers::GetCameraSpeedMultiplier(Client);
		if (ImGui::SliderFloat("Speed", &SpeedMultiplier, 0.01f, FEditorMainPanelViewportToolbarHelpers::MaxCameraSpeedMultiplier, "%.2fx"))
		{
			FEditorMainPanelViewportToolbarHelpers::SetCameraSpeedMultiplier(Client, SpeedMultiplier);
		}
		ImGui::EndPopup();
	}

	ImGui::SameLine();
	const char* ViewModeName = ViewportState
		? FEditorMainPanelViewportToolbarHelpers::GetViewModeName(ViewportState->ViewMode)
		: "View";
	char ViewButtonLabel[80];
	snprintf(ViewButtonLabel, sizeof(ViewButtonLabel), "%s ▼", ViewModeName);
	if (DrawViewportTextButton("##ViewerViewModeShared", ViewButtonLabel))
	{
		ImGui::OpenPopup("##ViewerViewModePopupShared");
	}
	if (ViewportState && ImGui::BeginPopup("##ViewerViewModePopupShared"))
	{
		FEditorMainPanelViewportToolbarHelpers::ForEachViewMode(
			[ViewportState](EViewMode Mode)
			{
				if (ImGui::MenuItem(
					FEditorMainPanelViewportToolbarHelpers::GetViewModeName(Mode),
					nullptr,
					ViewportState->ViewMode == Mode))
				{
					ViewportState->ViewMode = Mode;
				}
			});
		ImGui::Separator();
		if (ImGui::BeginMenu("Light Culling"))
		{
			static constexpr ELightCullMode CullModes[] = {
				ELightCullMode::Clustered,
				ELightCullMode::Tiled,
				ELightCullMode::None,
			};
			for (ELightCullMode CullMode : CullModes)
			{
				if (ImGui::MenuItem(
					FEditorMainPanelViewportToolbarHelpers::GetLightCullModeName(CullMode),
					nullptr,
					ViewportState->LightCullMode == CullMode))
				{
					ViewportState->LightCullMode = CullMode;
				}
			}
			ImGui::EndMenu();
		}
		ImGui::EndPopup();
	}

	ImGui::SameLine(0.0f, 10.0f);
	if (DrawViewportTextButton("##ViewerShowFlagsShared", "Show ▼"))
	{
		ImGui::OpenPopup("##ViewerShowFlagsPopupShared");
	}
	if (ImGui::BeginPopup("##ViewerShowFlagsPopupShared"))
	{
		if (ShowFlags)
		{
			ImGui::MenuItem("Skeletal Mesh", nullptr, &ShowFlags->bShowSkeletalMesh);
			ImGui::MenuItem("Bones", nullptr, &ShowFlags->bShowBones);
			ImGui::BeginDisabled(!ShowFlags->bShowBones);
			ImGui::MenuItem("Selected Bone Only", nullptr, &ShowFlags->bShowOnlySelectedBone);
			ImGui::EndDisabled();
			ImGui::Separator();
			ImGui::MenuItem("Bounding Box", nullptr, &ShowFlags->bShowBoundingBox);
			ImGui::MenuItem("Outline", nullptr, &ShowFlags->bShowOutline);
		}
		ImGui::EndPopup();
	}
	ImGui::PopID();
}

void FEditorMainPanel::RenderViewerTransformToolbarControls(FEditorViewer* Viewer)
{
	if (!Viewer)
	{
		return;
	}

	FEditorViewportClient* Client = &Viewer->GetClient();
	ImGui::PushID(Viewer);
	if (DrawViewportIconButton(
		"##ViewerSelectModeShared",
		EEditorMainPanelViewportToolIcon::Select,
		"Q",
		"Select (Q / 1)",
		Client->GetTransformMode() == FEditorViewportClient::ETransformMode::Select,
		true))
	{
		Client->RequestSetSelectMode();
	}
	ImGui::SameLine();
	if (DrawViewportIconButton(
		"##ViewerTranslateModeShared",
		EEditorMainPanelViewportToolIcon::Translate,
		"W",
		"Translate (W / 2)",
		Client->GetTransformMode() == FEditorViewportClient::ETransformMode::Translate,
		true))
	{
		Client->RequestSetTranslateMode();
	}
	ImGui::SameLine();
	if (DrawViewportIconButton(
		"##ViewerRotateModeShared",
		EEditorMainPanelViewportToolIcon::Rotate,
		"E",
		"Rotate (E / 3)",
		Client->GetTransformMode() == FEditorViewportClient::ETransformMode::Rotate,
		true))
	{
		Client->RequestSetRotateMode();
	}
	ImGui::SameLine();
	if (DrawViewportIconButton(
		"##ViewerScaleModeShared",
		EEditorMainPanelViewportToolIcon::Scale,
		"R",
		"Scale (R / 4)",
		Client->GetTransformMode() == FEditorViewportClient::ETransformMode::Scale,
		true))
	{
		Client->RequestSetScaleMode();
	}
	ImGui::PopID();
}

bool FEditorMainPanel::RenderActiveDocumentMainMenu()
{
	const FEditorTabEntry* ActiveTab = EditorTabs.GetActiveTab();
	if (!ActiveTab || ActiveTab->Id.Kind == EEditorTabKind::LevelEditor || ActiveTab->bDetached)
	{
		return false;
	}

	auto RenderCommonDocumentWindowMenu = [this, ActiveTab]()
	{
		if (!ImGui::BeginMenu("Window"))
		{
			return;
		}

		if (ActiveTab->bCanClose && ImGui::MenuItem("Detach Tab"))
		{
			RequestDetachEditorTab(ActiveTab->Id, true);
		}
		ImGui::MenuItem("Content Browser", "Ctrl+Space", &PanelVisibility.bShowContentBrowser);
		ImGui::EndMenu();
	};

	auto RenderDocumentHelpMenu = []()
	{
		if (!ImGui::BeginMenu("Help"))
		{
			return;
		}
		ImGui::TextDisabled("Document-specific editor context");
		ImGui::EndMenu();
	};

	if (ActiveTab->Id.Kind == EEditorTabKind::SkeletalMeshViewer ||
		ActiveTab->Id.Kind == EEditorTabKind::StaticMeshViewer ||
		ActiveTab->Id.Kind == EEditorTabKind::AnimSequenceViewer)
	{
		FEditorViewerWidget* ViewerWidget = FindViewerWidgetForTab(ActiveTab->Id);
		FEditorViewer* Viewer = ViewerWidget ? ViewerWidget->GetViewer() : nullptr;
		const bool bCanSaveMesh = ViewerWidget && ViewerWidget->CanSaveMesh();
		const char* SaveLabel = ViewerWidget && ViewerWidget->IsMeshDirty() ? "Save Mesh *" : "Save Mesh";

		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem(SaveLabel, "Ctrl+S", false, bCanSaveMesh))
			{
				ViewerWidget->RequestSaveMesh();
			}
			ImGui::Separator();
			if (ActiveTab->bCanClose && ImGui::MenuItem("Close Tab"))
			{
				RequestCloseEditorTab(ActiveTab->Id);
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Edit"))
		{
			ImGui::MenuItem("Undo", "Ctrl+Z", false, false);
			ImGui::MenuItem("Redo", "Ctrl+Shift+Z", false, false);
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Asset"))
		{
			if (ImGui::MenuItem(SaveLabel, nullptr, false, bCanSaveMesh))
			{
				ViewerWidget->RequestSaveMesh();
			}
			ImGui::MenuItem("Reimport Mesh", nullptr, false, false);
			if (Viewer)
			{
				ImGui::Separator();
				ImGui::TextDisabled("%s", Viewer->GetFileName().c_str());
			}
			ImGui::EndMenu();
		}

		RenderCommonDocumentWindowMenu();

		if (ImGui::BeginMenu("Tools"))
		{
			if (Viewer)
			{
				FEditorViewportClient& Client = Viewer->GetClient();
				if (ImGui::MenuItem("Select", "Q / 1", Client.GetTransformMode() == FEditorViewportClient::ETransformMode::Select))
				{
					Client.RequestSetSelectMode();
				}
				if (ImGui::MenuItem("Translate", "W / 2", Client.GetTransformMode() == FEditorViewportClient::ETransformMode::Translate))
				{
					Client.RequestSetTranslateMode();
				}
				if (ImGui::MenuItem("Rotate", "E / 3", Client.GetTransformMode() == FEditorViewportClient::ETransformMode::Rotate))
				{
					Client.RequestSetRotateMode();
				}
				if (ImGui::MenuItem("Scale", "R / 4", Client.GetTransformMode() == FEditorViewportClient::ETransformMode::Scale))
				{
					Client.RequestSetScaleMode();
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Reset Preview Camera"))
				{
					Client.ResetCamera();
					Client.ApplyCameraMode();
				}
				ImGui::Separator();
				if (FSkeletalAssetEditorViewer* SkeletalViewer = AsSkeletalAssetViewer(Viewer))
				{
					FSkeletalMeshViewerShowFlags& ShowFlags = SkeletalViewer->GetClient().GetShowFlags();
					ImGui::MenuItem("Skeletal Mesh", nullptr, &ShowFlags.bShowSkeletalMesh);
					ImGui::MenuItem("Bones", nullptr, &ShowFlags.bShowBones);
					ImGui::BeginDisabled(!ShowFlags.bShowBones);
					ImGui::MenuItem("Selected Bone Only", nullptr, &ShowFlags.bShowOnlySelectedBone);
					ImGui::EndDisabled();
					ImGui::MenuItem("Bounding Box", nullptr, &ShowFlags.bShowBoundingBox);
					ImGui::MenuItem("Outline", nullptr, &ShowFlags.bShowOutline);
				}
			}
			else
			{
				ImGui::TextDisabled("Viewer is not ready.");
			}
			ImGui::EndMenu();
		}

		RenderDocumentHelpMenu();
		return true;
	}

	if (ImGui::BeginMenu("File"))
	{
		const bool bAnimGraph = ActiveTab->Id.Kind == EEditorTabKind::AnimGraphEditor;
		const char* SaveLabel = bAnimGraph && Widgets.AnimGraphWidget.IsDirty() ? "Save Asset *" : "Save Asset";
		if (ImGui::MenuItem(SaveLabel, "Ctrl+S", false, bAnimGraph))
		{
			Widgets.AnimGraphWidget.Save();
			EditorTabs.SetTabDirty(ActiveTab->Id, Widgets.AnimGraphWidget.IsDirty());
		}
		ImGui::Separator();
		if (ActiveTab->bCanClose && ImGui::MenuItem("Close Tab"))
		{
			RequestCloseEditorTab(ActiveTab->Id);
		}
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Edit"))
	{
		ImGui::MenuItem("Undo", "Ctrl+Z", false, false);
		ImGui::MenuItem("Redo", "Ctrl+Shift+Z", false, false);
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Asset"))
	{
		switch (ActiveTab->Id.Kind)
		{
		case EEditorTabKind::MaterialEditor:
			ImGui::TextDisabled("Material Editor");
			break;
		case EEditorTabKind::CurveEditor:
			ImGui::TextDisabled("Curve Editor");
			break;
		case EEditorTabKind::ActorSequencer:
			ImGui::TextDisabled("Actor Sequencer");
			break;
		case EEditorTabKind::RuntimeUIPreview:
			ImGui::TextDisabled("Runtime UI Preview");
			break;
		case EEditorTabKind::AnimGraphEditor:
			ImGui::TextDisabled("Anim Graph");
			if (!Widgets.AnimGraphWidget.GetEditingPath().empty())
			{
				ImGui::Separator();
				ImGui::TextDisabled("%s", Widgets.AnimGraphWidget.GetEditingPath().c_str());
			}
			break;
		default:
			ImGui::TextDisabled("Document");
			break;
		}
		ImGui::EndMenu();
	}

	RenderCommonDocumentWindowMenu();
	RenderDocumentHelpMenu();
	return true;
}

