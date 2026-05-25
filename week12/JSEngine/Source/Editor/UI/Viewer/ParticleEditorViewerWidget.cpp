#include "ParticleEditorViewerWidget.h"

#include "Core/Reflection/ReflectionRegistry.h"
#include "Editor/EditorEngine.h"
#include "Editor/UI/EditorMainPanel.h"
#include "Editor/UI/EditorMainPanelViewportToolbarHelpers.h"
#include "Editor/Viewer/EditorViewer.h"
#include "Editor/Viewer/ParticleEditorViewer.h"
#include "Editor/Viewport/FSceneViewport.h"
#include "Engine/Core/EditorResourcePaths.h"
#include "Object/Class.h"
#include "Particle/ParticleAsset.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "WICTextureLoader.h"

#include <algorithm>
#include <cstring>

namespace
{
	constexpr const char* ParticleModuleDragPayload = "ParticleModule";
	constexpr const char* ParticleEmitterDragPayload = "ParticleEmitter";
	constexpr float EmitterNodeWidth = 220.0f;
	constexpr float EmitterSeparatorGap = 10.0f;

	struct FParticleModuleDragPayload
	{
		int32 EmitterIndex = -1;
		int32 LODIndex = -1;
		int32 ModuleIndex = -1;
	};

	struct FParticleEmitterDragPayload
	{
		int32 EmitterIndex = -1;
	};

	FParticleEditorViewer* AsParticleViewer(FEditorViewer* Viewer);
	const char* GetSelectionLabel(EParticleEditorSelectionType Type);
	const char* GetObjectLabel(const UObject* Object);
	void GetParticleModuleClasses(TArray<UClass*>& OutClasses);
	bool DrawParticleModuleClassMenu(FParticleEditorViewer* Viewer);
	void DrawViewModeMenuItems(FParticleEditorViewer* Viewer);
	bool DrawPopupButton(const char* Label, const char* PopupId);
	bool DrawRoundedToolbarButton(const char* Id, const char* Label, const char* Tooltip, const ImVec2& Size);
	void DrawParticlePanelTitle(const char* Title, const char* Subtitle);
	void DrawParticleDetailsSection(const char* Title);
	void DrawParticleDetailsText(const char* Label, const char* Value);
	void DrawEmitterPreview(const ImVec2& Size, int32 EmitterIndex, bool bSelected);
	void DrawSelectableModuleRow(FParticleEditorViewer* Viewer, const char* Label, EParticleEditorSelectionType Type, int32 EmitterIndex, int32 LODIndex, int32 ModuleIndex, ImU32 BackgroundColor);
}

void FParticleEditorViewerWidget::RenderContent(float DeltaTime)
{
	(void)DeltaTime;

	FParticleEditorViewer* ParticleViewer = AsParticleViewer(Viewer);
	if (!ParticleViewer)
	{
		FEditorViewerWidget::RenderContent(DeltaTime);
		return;
	}

	FSceneViewport& SceneViewport = ParticleViewer->GetViewport();
	ID3D11ShaderResourceView* SRV = SceneViewport.GetOutSRV();
	if (!SRV)
	{
		ImGui::TextDisabled("Viewer render target is not ready.");
		return;
	}

	RenderToolbar(ParticleViewer);

	const ImVec2 FullSize = ImGui::GetContentRegionAvail();
	if (FullSize.x <= 0.0f || FullSize.y <= 0.0f)
	{
		return;
	}

	ImGui::PushID(ParticleViewer);

	const float SplitterThickness = 4.0f;
	const float SplitterSideGap = 6.0f;
	const float SplitterTotalWidth = SplitterThickness + SplitterSideGap * 2.0f;

	const float MinColumnWidth = std::min(220.0f, std::max(80.0f, (FullSize.x - SplitterThickness) * 0.25f));
	const float MinPanelHeight = std::min(140.0f, std::max(60.0f, (FullSize.y - SplitterThickness) * 0.25f));
	EmitterPanelWidthRatio = std::clamp(EmitterPanelWidthRatio, 0.2f, 0.85f);
	BottomPanelHeightRatio = std::clamp(BottomPanelHeightRatio, 0.2f, 0.8f);

	float RightWidth = FullSize.x * EmitterPanelWidthRatio;
	RightWidth = std::clamp(RightWidth, MinColumnWidth, std::max(MinColumnWidth, FullSize.x - MinColumnWidth - SplitterThickness));
	float BottomHeight = FullSize.y * BottomPanelHeightRatio;
	BottomHeight = std::clamp(BottomHeight, MinPanelHeight, std::max(MinPanelHeight, FullSize.y - MinPanelHeight - SplitterThickness));
	const float LeftWidth = std::max(MinColumnWidth, FullSize.x - RightWidth - SplitterThickness);
	const float TopHeight = std::max(MinPanelHeight, FullSize.y - BottomHeight - SplitterThickness);

	ImGui::BeginGroup();
	if (ImGui::BeginChild("ViewportPanel", ImVec2(LeftWidth, TopHeight), true))
	{
		DrawParticlePanelTitle("Viewport", "Preview");
		RenderViewportPanel(SceneViewport, SRV, ImGui::GetContentRegionAvail());
		if (BeginViewportToolbar(false))
		{
			ImGui::PushID(ParticleViewer);
			RenderViewportOptions(ParticleViewer);
			ImGui::PopID();
			EndViewportToolbar();
		}
	}
	ImGui::EndChild();

	ImGui::Button("##ParticleLeftHorizontalSplitter", ImVec2(LeftWidth, SplitterThickness));
	if (ImGui::IsItemActive())
	{
		BottomHeight = std::clamp(BottomHeight - ImGui::GetIO().MouseDelta.y, MinPanelHeight, std::max(MinPanelHeight, FullSize.y - MinPanelHeight - SplitterThickness));
		BottomPanelHeightRatio = BottomHeight / FullSize.y;
	}

	if (ImGui::BeginChild("DetailsPanel", ImVec2(LeftWidth, 0.0f), true))
	{
		RenderDetailsPanel(ParticleViewer);
	}
	ImGui::EndChild();
	ImGui::EndGroup();

	ImGui::SameLine();
	ImGui::Button("##ParticleVerticalSplitter", ImVec2(SplitterThickness, FullSize.y));
	if (ImGui::IsItemActive())
	{
		RightWidth = std::clamp(RightWidth - ImGui::GetIO().MouseDelta.x, MinColumnWidth, std::max(MinColumnWidth, FullSize.x - MinColumnWidth - SplitterTotalWidth));
		EmitterPanelWidthRatio = RightWidth / FullSize.x;
	}
	ImGui::SameLine();

	ImGui::BeginGroup();
	if (ImGui::BeginChild("EmittersPanel", ImVec2(RightWidth, TopHeight), true))
	{
		RenderEmitterPanel(ParticleViewer);
	}
	ImGui::EndChild();

	ImGui::Button("##ParticleRightHorizontalSplitter", ImVec2(RightWidth, SplitterThickness));
	if (ImGui::IsItemActive())
	{
		BottomHeight = std::clamp(BottomHeight - ImGui::GetIO().MouseDelta.y, MinPanelHeight, std::max(MinPanelHeight, FullSize.y - MinPanelHeight - SplitterThickness));
		BottomPanelHeightRatio = BottomHeight / FullSize.y;
	}

	if (ImGui::BeginChild("CurveEditorPanel", ImVec2(RightWidth, 0.0f), true))
	{
		RenderCurveEditor(ParticleViewer);
	}
	ImGui::EndChild();
	ImGui::EndGroup();

	ImGui::PopID();
}

void FParticleEditorViewerWidget::RenderMenuBar(FParticleEditorViewer* Viewer)
{
	ImGui::BeginChild("ParticleMenuBar", ImVec2(0.0f, 30.0f), false, ImGuiWindowFlags_NoScrollbar);
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);

	if (DrawPopupButton("File", "##ParticleFileMenu"))
	{
		if (ImGui::BeginPopup("##ParticleFileMenu"))
		{
			if (ImGui::MenuItem("Save", "Ctrl+S", false, Viewer->IsDirty()))
			{
				Viewer->Save();
			}
			ImGui::EndPopup();
		}
	}
	ImGui::SameLine();
	if (DrawPopupButton("Edit", "##ParticleEditMenu"))
	{
		if (ImGui::BeginPopup("##ParticleEditMenu"))
		{
			ImGui::MenuItem("Undo", "Ctrl+Z", false, false);
			ImGui::MenuItem("Redo", "Ctrl+Shift+Z", false, false);
			ImGui::EndPopup();
		}
	}
	ImGui::SameLine();
	if (DrawPopupButton("Asset", "##ParticleAssetMenu"))
	{
		if (ImGui::BeginPopup("##ParticleAssetMenu"))
		{
			if (ImGui::MenuItem("Find in Content Browser"))
			{
				Viewer->FindInContentBrowser();
			}
			if (ImGui::MenuItem("Restart Simulation"))
			{
				Viewer->RestartSimulation();
			}
			if (ImGui::MenuItem("Restart Level"))
			{
				Viewer->RestartLevel();
			}
			ImGui::EndPopup();
		}
	}
	ImGui::SameLine();
	if (DrawPopupButton("Window", "##ParticleWindowMenu"))
	{
		if (ImGui::BeginPopup("##ParticleWindowMenu"))
		{
			bool bGrid = Viewer->IsShowGrid();
			if (ImGui::MenuItem("Grid", nullptr, bGrid))
			{
				Viewer->SetShowGrid(!bGrid);
			}
			bool bBounds = Viewer->IsShowBounds();
			if (ImGui::MenuItem("Particle System Bounds", nullptr, bBounds))
			{
				Viewer->SetShowBounds(!bBounds);
			}
			ImGui::Separator();
			float EmitterRatio = EmitterPanelWidthRatio;
			if (ImGui::SliderFloat("Emitter Width", &EmitterRatio, 0.2f, 0.85f, "%.2f"))
			{
				EmitterPanelWidthRatio = EmitterRatio;
			}
			float BottomRatio = BottomPanelHeightRatio;
			if (ImGui::SliderFloat("Bottom Height", &BottomRatio, 0.2f, 0.8f, "%.2f"))
			{
				BottomPanelHeightRatio = BottomRatio;
			}
			if (ImGui::MenuItem("Reset Particle Layout"))
			{
				EmitterPanelWidthRatio = 2.0f / 3.0f;
				BottomPanelHeightRatio = 0.5f;
			}
			ImGui::EndPopup();
		}
	}
	ImGui::SameLine();
	if (DrawPopupButton("Help", "##ParticleHelpMenu"))
	{
		if (ImGui::BeginPopup("##ParticleHelpMenu"))
		{
			ImGui::TextDisabled("Particle System Viewer");
			ImGui::EndPopup();
		}
	}

	ImGui::PopStyleVar();
	ImGui::EndChild();
}

void FParticleEditorViewerWidget::RenderToolbar(FParticleEditorViewer* Viewer)
{
	LoadCascadeToolbarIcons();

	constexpr ImGuiWindowFlags ToolbarFlags =
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse;
	ImGui::BeginChild("ParticleToolbar", ImVec2(0.0f, 34.0f), false, ToolbarFlags);
	ImGui::SetCursorPos(ImVec2(8.0f, 4.0f));
	const ImVec2 IconSize(26.0f, 26.0f);
	const float OverflowButtonWidth = IconSize.y;
	const float VisibleRight = ImGui::GetWindowContentRegionMax().x - OverflowButtonWidth - 8.0f;
	bool bHasOverflow = false;
	bool bOpenBackgroundPopup = false;
	bool bHiddenSave = false;
	bool bHiddenFind = false;
	bool bHiddenRestartSim = false;
	bool bHiddenRestartLevel = false;
	bool bHiddenUndo = false;
	bool bHiddenRedo = false;
	bool bHiddenThumbnail = false;
	bool bHiddenBounds = false;
	bool bHiddenAxis = false;
	bool bHiddenBackground = false;
	bool bHiddenRegenLOD = false;
	bool bHiddenLowestLOD = false;
	bool bHiddenLowerLOD = false;
	bool bHiddenAddLOD = false;
	bool bHiddenCurrentLOD = false;
	bool bHiddenUpperLOD = false;
	bool bHiddenHighestLOD = false;

	auto DrawBackgroundColorPopup = [&]()
	{
		if (ImGui::BeginPopup("ParticleBackgroundColorPopup"))
		{
			FColor Background = Viewer->GetBackgroundColor();
			float Color[4] = { Background.R, Background.G, Background.B, Background.A };
			if (ImGui::ColorPicker4("##ParticleBackgroundColor", Color, ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview))
			{
				Viewer->SetBackgroundColor(FColor(Color[0], Color[1], Color[2], Color[3]));
			}
			ImGui::EndPopup();
		}
	};

	auto EstimateButtonWidth = [IconSize](const char* Label)
	{
		return IconSize.x + (Label ? 14.0f + ImGui::CalcTextSize(Label).x : 0.0f);
	};
	auto CanFit = [VisibleRight](float Width)
	{
		return ImGui::GetCursorPosX() + Width <= VisibleRight;
	};
	auto DrawSeparatorIfFits = [&]()
	{
		constexpr float SeparatorWidth = 14.0f;
		if (!CanFit(SeparatorWidth))
		{
			bHasOverflow = true;
			return;
		}
		ImGui::SameLine();
		ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
		ImGui::SameLine();
	};
	auto DrawVisibleButton = [&](const char* Id, ID3D11ShaderResourceView* Icon, const char* Tooltip, bool bEnabled, const char* Label, bool& bHidden, auto&& OnClick)
	{
		const float Width = EstimateButtonWidth(Label);
		if (!CanFit(Width))
		{
			bHasOverflow = true;
			bHidden = true;
			return;
		}
		bHidden = false;
		if (DrawCascadeToolbarIconButton(Id, Icon, Tooltip, IconSize, bEnabled, Label))
		{
			OnClick();
		}
		ImGui::SameLine();
	};

	DrawVisibleButton("Save", CascadeSaveIcon.Get(), "Save", Viewer->IsDirty(), nullptr, bHiddenSave, [&]() { Viewer->Save(); });
	DrawVisibleButton("Find", CascadeFindIcon.Get(), "Find in Content Browser", true, nullptr, bHiddenFind, [&]() { Viewer->FindInContentBrowser(); });
	DrawSeparatorIfFits();
	DrawVisibleButton("RestartSim", CascadeRestartSimIcon.Get(), "Restart Simulation", true, "Restart Sim", bHiddenRestartSim, [&]() { Viewer->RestartSimulation(); });
	DrawVisibleButton("RestartLevel", CascadeRestartLevelIcon.Get(), "Restart Level", true, "Restart Level", bHiddenRestartLevel, [&]() { Viewer->RestartLevel(); });
	DrawSeparatorIfFits();
	DrawVisibleButton("Undo", CascadeUndoIcon.Get(), "Undo", false, "Undo", bHiddenUndo, []() {});
	DrawVisibleButton("Redo", CascadeRedoIcon.Get(), "Redo", false, "Redo", bHiddenRedo, []() {});
	DrawSeparatorIfFits();
	DrawVisibleButton("Thumbnail", CascadeThumbnailIcon.Get(), "Thumbnail", false, "Thumbnail", bHiddenThumbnail, []() {});
	DrawSeparatorIfFits();
	DrawVisibleButton("Bounds", CascadeBoundsIcon.Get(), Viewer->IsShowBounds() ? "Hide Bounds" : "Show Bounds", true, "Bounds", bHiddenBounds, [&]() { Viewer->SetShowBounds(!Viewer->IsShowBounds()); });
	DrawVisibleButton("Axis", CascadeAxisIcon.Get(), Viewer->IsShowGrid() ? "Hide Axis/Grid" : "Show Axis/Grid", true, "Axis", bHiddenAxis, [&]() { Viewer->SetShowGrid(!Viewer->IsShowGrid()); });
	DrawVisibleButton("Background", CascadeBackgroundIcon.Get(), "Background", true, "Background", bHiddenBackground, [&]() { bOpenBackgroundPopup = true; });
	DrawSeparatorIfFits();
	DrawVisibleButton("RegenLOD", CascadeRegenLODIcon.Get(), "Regenerate LOD", false, "Regen LOD", bHiddenRegenLOD, []() {});
	DrawVisibleButton("LowestLOD", CascadeLowestLODIcon.Get(), "Lowest LOD", true, "Lowest LOD", bHiddenLowestLOD, [&]() { Viewer->SetLowestLOD(); });
	DrawVisibleButton("LowerLOD", CascadeLowerLODIcon.Get(), "Lower LOD", true, "Lower LOD", bHiddenLowerLOD, [&]() { Viewer->SelectLowerLOD(); });
	DrawVisibleButton("AddLOD", CascadeAddLODIcon.Get(), "Add LOD", true, "Add LOD", bHiddenAddLOD, [&]() { Viewer->AddLOD(); });
	const FString LODLabel = "LOD: " + std::to_string(std::max(0, Viewer->GetSelectedLODIndex()));
	DrawVisibleButton("CurrentLOD", CascadeGenericLODIcon.Get(), "Current LOD", false, LODLabel.c_str(), bHiddenCurrentLOD, []() {});
	DrawVisibleButton("UpperLOD", CascadeUpperLODIcon.Get(), "Upper LOD", true, "Upper LOD", bHiddenUpperLOD, [&]() { Viewer->SelectUpperLOD(); });
	DrawVisibleButton("HighestLOD", CascadeHighestLODIcon.Get(), "Highest LOD", true, "Highest LOD", bHiddenHighestLOD, [&]() { Viewer->SetHighestLOD(); });

	if (bHasOverflow)
	{
		ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowContentRegionMax().x - OverflowButtonWidth));
		if (ImGui::InvisibleButton("##ParticleToolbarOverflow", ImVec2(OverflowButtonWidth, OverflowButtonWidth)))
		{
			ImGui::OpenPopup("ParticleToolbarOverflowPopup");
		}
		const ImVec2 OverflowMin = ImGui::GetItemRectMin();
		const ImVec2 OverflowMax = ImGui::GetItemRectMax();
		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		const ImU32 OverflowBg = ImGui::GetColorU32(ImGui::IsItemHovered() ? ImVec4(0.14f, 0.16f, 0.19f, 1.0f) : ImVec4(0.09f, 0.10f, 0.12f, 1.0f));
		const ImU32 OverflowFg = ImGui::GetColorU32(ImVec4(0.94f, 0.95f, 0.98f, 1.0f));
		const ImU32 OverflowBorder = ImGui::GetColorU32(ImGui::IsItemHovered() ? ImVec4(0.48f, 0.52f, 0.60f, 1.0f) : ImVec4(0.30f, 0.33f, 0.39f, 1.0f));
		DrawList->AddRectFilled(OverflowMin, OverflowMax, OverflowBg, 3.0f);
		DrawList->AddRect(OverflowMin, OverflowMax, OverflowBorder, 3.0f, 0, 1.0f);
		for (int32 Line = 0; Line < 3; ++Line)
		{
			const float Y = OverflowMin.y + 7.0f + Line * 5.0f;
			DrawList->AddLine(ImVec2(OverflowMin.x + 7.0f, Y), ImVec2(OverflowMax.x - 7.0f, Y), OverflowFg, 1.4f);
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("More particle tools");
		}
	}

	constexpr ImGuiWindowFlags OverflowPopupFlags =
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse;
	if (ImGui::BeginPopup("ParticleToolbarOverflowPopup", OverflowPopupFlags))
	{
		const ImVec2 PopupIconSize(26.0f, 26.0f);
		auto DrawOverflowButton = [&](const char* Id, ID3D11ShaderResourceView* Icon, const char* Tooltip, bool bEnabled, const char* Label, auto&& OnClick)
		{
			if (DrawCascadeToolbarIconButton(Id, Icon, Tooltip, PopupIconSize, bEnabled, Label))
			{
				OnClick();
				ImGui::CloseCurrentPopup();
			}
		};
		auto DrawSeparatorForHiddenGroup = [](bool bAnyHidden)
		{
			if (bAnyHidden)
			{
				ImGui::Separator();
			}
		};

		bool bNeedsSeparator = false;
		if (bHiddenSave)
		{
			DrawOverflowButton("OverflowSave", CascadeSaveIcon.Get(), "Save", Viewer->IsDirty(), "Save", [&]() { Viewer->Save(); });
			bNeedsSeparator = true;
		}
		if (bHiddenFind)
		{
			DrawOverflowButton("OverflowFind", CascadeFindIcon.Get(), "Find in Content Browser", true, "Find", [&]() { Viewer->FindInContentBrowser(); });
			bNeedsSeparator = true;
		}
		DrawSeparatorForHiddenGroup(bNeedsSeparator && (bHiddenRestartSim || bHiddenRestartLevel || bHiddenUndo || bHiddenRedo || bHiddenThumbnail || bHiddenBounds || bHiddenAxis || bHiddenBackground || bHiddenRegenLOD || bHiddenLowestLOD || bHiddenLowerLOD || bHiddenAddLOD || bHiddenCurrentLOD || bHiddenUpperLOD || bHiddenHighestLOD));

		bNeedsSeparator = false;
		if (bHiddenRestartSim)
		{
			DrawOverflowButton("OverflowRestartSim", CascadeRestartSimIcon.Get(), "Restart Simulation", true, "Restart Sim", [&]() { Viewer->RestartSimulation(); });
			bNeedsSeparator = true;
		}
		if (bHiddenRestartLevel)
		{
			DrawOverflowButton("OverflowRestartLevel", CascadeRestartLevelIcon.Get(), "Restart Level", true, "Restart Level", [&]() { Viewer->RestartLevel(); });
			bNeedsSeparator = true;
		}
		DrawSeparatorForHiddenGroup(bNeedsSeparator && (bHiddenUndo || bHiddenRedo || bHiddenThumbnail || bHiddenBounds || bHiddenAxis || bHiddenBackground || bHiddenRegenLOD || bHiddenLowestLOD || bHiddenLowerLOD || bHiddenAddLOD || bHiddenCurrentLOD || bHiddenUpperLOD || bHiddenHighestLOD));

		bNeedsSeparator = false;
		if (bHiddenUndo)
		{
			DrawOverflowButton("OverflowUndo", CascadeUndoIcon.Get(), "Undo", false, "Undo", []() {});
			bNeedsSeparator = true;
		}
		if (bHiddenRedo)
		{
			DrawOverflowButton("OverflowRedo", CascadeRedoIcon.Get(), "Redo", false, "Redo", []() {});
			bNeedsSeparator = true;
		}
		DrawSeparatorForHiddenGroup(bNeedsSeparator && (bHiddenThumbnail || bHiddenBounds || bHiddenAxis || bHiddenBackground || bHiddenRegenLOD || bHiddenLowestLOD || bHiddenLowerLOD || bHiddenAddLOD || bHiddenCurrentLOD || bHiddenUpperLOD || bHiddenHighestLOD));

		if (bHiddenThumbnail)
		{
			DrawOverflowButton("OverflowThumbnail", CascadeThumbnailIcon.Get(), "Thumbnail", false, "Thumbnail", []() {});
			DrawSeparatorForHiddenGroup(bHiddenBounds || bHiddenAxis || bHiddenBackground || bHiddenRegenLOD || bHiddenLowestLOD || bHiddenLowerLOD || bHiddenAddLOD || bHiddenCurrentLOD || bHiddenUpperLOD || bHiddenHighestLOD);
		}

		bNeedsSeparator = false;
		if (bHiddenBounds)
		{
			DrawOverflowButton("OverflowBounds", CascadeBoundsIcon.Get(), Viewer->IsShowBounds() ? "Hide Bounds" : "Show Bounds", true, "Bounds", [&]() { Viewer->SetShowBounds(!Viewer->IsShowBounds()); });
			bNeedsSeparator = true;
		}
		if (bHiddenAxis)
		{
			DrawOverflowButton("OverflowAxis", CascadeAxisIcon.Get(), Viewer->IsShowGrid() ? "Hide Axis/Grid" : "Show Axis/Grid", true, "Axis", [&]() { Viewer->SetShowGrid(!Viewer->IsShowGrid()); });
			bNeedsSeparator = true;
		}
		if (bHiddenBackground)
		{
			DrawOverflowButton("OverflowBackground", CascadeBackgroundIcon.Get(), "Background", true, "Background", [&]() { bOpenBackgroundPopup = true; });
			bNeedsSeparator = true;
		}
		DrawSeparatorForHiddenGroup(bNeedsSeparator && (bHiddenRegenLOD || bHiddenLowestLOD || bHiddenLowerLOD || bHiddenAddLOD || bHiddenCurrentLOD || bHiddenUpperLOD || bHiddenHighestLOD));

		if (bHiddenRegenLOD)
		{
			DrawOverflowButton("OverflowRegenLOD", CascadeRegenLODIcon.Get(), "Regenerate LOD", false, "Regen LOD", []() {});
		}
		if (bHiddenLowestLOD)
		{
			DrawOverflowButton("OverflowLowestLOD", CascadeLowestLODIcon.Get(), "Lowest LOD", true, "Lowest LOD", [&]() { Viewer->SetLowestLOD(); });
		}
		if (bHiddenLowerLOD)
		{
			DrawOverflowButton("OverflowLowerLOD", CascadeLowerLODIcon.Get(), "Lower LOD", true, "Lower LOD", [&]() { Viewer->SelectLowerLOD(); });
		}
		if (bHiddenAddLOD)
		{
			DrawOverflowButton("OverflowAddLOD", CascadeAddLODIcon.Get(), "Add LOD", true, "Add LOD", [&]() { Viewer->AddLOD(); });
		}
		if (bHiddenCurrentLOD)
		{
			DrawOverflowButton("OverflowCurrentLOD", CascadeGenericLODIcon.Get(), "Current LOD", false, LODLabel.c_str(), []() {});
		}
		if (bHiddenUpperLOD)
		{
			DrawOverflowButton("OverflowUpperLOD", CascadeUpperLODIcon.Get(), "Upper LOD", true, "Upper LOD", [&]() { Viewer->SelectUpperLOD(); });
		}
		if (bHiddenHighestLOD)
		{
			DrawOverflowButton("OverflowHighestLOD", CascadeHighestLODIcon.Get(), "Highest LOD", true, "Highest LOD", [&]() { Viewer->SetHighestLOD(); });
		}
		ImGui::EndPopup();
	}
	if (bOpenBackgroundPopup)
	{
		ImGui::OpenPopup("ParticleBackgroundColorPopup");
	}
	DrawBackgroundColorPopup();

	ImGui::EndChild();
}

void FParticleEditorViewerWidget::RenderViewportOptions(FParticleEditorViewer* Viewer)
{
	if (DrawRoundedToolbarButton("ParticleViewportView", "View", "View", ImVec2(50.0f, 26.0f)))
	{
		ImGui::OpenPopup("##ParticleViewportViewPopup");
	}
	if (ImGui::BeginPopup("##ParticleViewportViewPopup"))
	{
		if (ImGui::BeginMenu("View Overlays"))
		{
			bool bGrid = Viewer->IsShowGrid();
			if (ImGui::MenuItem("Grid", nullptr, bGrid))
			{
				Viewer->SetShowGrid(!bGrid);
			}
			bool bBounds = Viewer->IsShowBounds();
			if (ImGui::MenuItem("Particle System Bounds", nullptr, bBounds))
			{
				Viewer->SetShowBounds(!bBounds);
			}
			ImGui::MenuItem("Particle Count", nullptr, false, false);
			ImGui::MenuItem("Distance", nullptr, false, false);
			ImGui::MenuItem("Elapsed Time", nullptr, false, false);
			ImGui::MenuItem("Memory", nullptr, false, false);
			ImGui::MenuItem("Event Count", nullptr, false, false);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("View Modes"))
		{
			DrawViewModeMenuItems(Viewer);
			ImGui::MenuItem("Shader Complexity", nullptr, false, false);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Detail Modes"))
		{
			ImGui::MenuItem("Low", nullptr, false, false);
			ImGui::MenuItem("Medium", nullptr, true, false);
			ImGui::MenuItem("High", nullptr, false, false);
			ImGui::EndMenu();
		}
		ImGui::EndPopup();
	}
	ImGui::SameLine(0.0f, 6.0f);
	if (DrawRoundedToolbarButton("ParticleViewportTime", "Time", "Time", ImVec2(52.0f, 26.0f)))
	{
		ImGui::OpenPopup("##ParticleViewportTimePopup");
	}
	if (ImGui::BeginPopup("##ParticleViewportTimePopup"))
	{
		RenderTimeControls(Viewer);
		ImGui::EndPopup();
	}
}

void FParticleEditorViewerWidget::RenderTimeControls(FParticleEditorViewer* Viewer)
{
	bool bPlaying = Viewer->IsPlaying();
	if (ImGui::MenuItem(bPlaying ? "Pause" : "Play", nullptr, bPlaying))
	{
		Viewer->SetPlaying(!bPlaying);
	}
	bool bRealtime = Viewer->IsRealtime();
	if (ImGui::MenuItem("Realtime", nullptr, bRealtime))
	{
		Viewer->SetRealtime(!bRealtime);
	}
	bool bLooping = Viewer->IsLooping();
	if (ImGui::MenuItem("Loop", nullptr, bLooping))
	{
		Viewer->SetLooping(!bLooping);
	}
}

void FParticleEditorViewerWidget::LoadCascadeToolbarIcons()
{
	if (bCascadeToolbarIconsLoadAttempted)
	{
		return;
	}

	bCascadeToolbarIconsLoadAttempted = true;
	if (!EditorEngine)
	{
		return;
	}

	ID3D11Device* Device = EditorEngine->GetRenderer().GetFD3DDevice().GetDevice();
	if (!Device)
	{
		return;
	}

	const std::wstring IconDir = FEditorResourcePaths::IconsAbsoluteDir();
	const std::wstring ToolIconDir = FEditorResourcePaths::ToolIconsAbsoluteDir();
	auto LoadIcon = [Device](const std::wstring& BaseDir, const wchar_t* FileName, TComPtr<ID3D11ShaderResourceView>& OutIcon)
	{
		const std::wstring IconPath = BaseDir + FileName;
		DirectX::CreateWICTextureFromFile(Device, IconPath.c_str(), nullptr, OutIcon.GetAddressOf());
	};

	LoadIcon(ToolIconDir, L"Save.png", CascadeSaveIcon);
	LoadIcon(ToolIconDir, L"Browser.png", CascadeFindIcon);
	LoadIcon(IconDir, L"Cascade_RestartSim_40x.png", CascadeRestartSimIcon);
	LoadIcon(IconDir, L"Cascade_Restart40x.png", CascadeRestartLevelIcon);
	LoadIcon(ToolIconDir, L"PlayControlsToPrevious.png", CascadeUndoIcon);
	LoadIcon(ToolIconDir, L"PlayControlsToNext.png", CascadeRedoIcon);
	LoadIcon(IconDir, L"Cascade_Bounds_40x.png", CascadeBoundsIcon);
	LoadIcon(IconDir, L"Cascade_Axis_40x.png", CascadeAxisIcon);
	LoadIcon(IconDir, L"Cascade_Color_40x.png", CascadeBackgroundIcon);
	LoadIcon(IconDir, L"Cascade_Thumbnail_40x.png", CascadeThumbnailIcon);
	LoadIcon(IconDir, L"Cascade_RegenLOD1_512x.png", CascadeRegenLODIcon);
	LoadIcon(IconDir, L"Cascade_LowestLOD_512x.png", CascadeLowestLODIcon);
	LoadIcon(IconDir, L"Cascade_HighestLOD_512x.png", CascadeHighestLODIcon);
	LoadIcon(IconDir, L"Cascade_LowerLOD_512x.png", CascadeLowerLODIcon);
	LoadIcon(IconDir, L"Cascade_HigherLOD_512x.png", CascadeUpperLODIcon);
	LoadIcon(IconDir, L"Cascade_AddLOD1_512x.png", CascadeAddLODIcon);
	LoadIcon(IconDir, L"Cascade_GenericLOD_40x.png", CascadeGenericLODIcon);
}

bool FParticleEditorViewerWidget::DrawCascadeToolbarIconButton(
	const char* Id,
	ID3D11ShaderResourceView* Icon,
	const char* Tooltip,
	const ImVec2& Size,
	bool bEnabled,
	const char* Label)
{
	ImGui::PushID(Id);
	if (!bEnabled)
	{
		ImGui::BeginDisabled();
	}

	const ImVec2 LabelSize = Label ? ImGui::CalcTextSize(Label) : ImVec2(0.0f, 0.0f);
	const float LabelGap = Label ? 6.0f : 0.0f;
	const ImVec2 ButtonSize(
		Size.x + LabelGap + LabelSize.x + (Label ? 8.0f : 0.0f),
		Size.y);
	const bool bClicked = ImGui::InvisibleButton("##CascadeToolbarIcon", ButtonSize);
	const ImVec2 Min = ImGui::GetItemRectMin();
	const ImVec2 Max = ImGui::GetItemRectMax();
	const bool bHovered = ImGui::IsItemHovered();
	const bool bActive = ImGui::IsItemActive();
	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	const ImU32 BgColor = ImGui::GetColorU32(
		bActive ? ImVec4(0.18f, 0.20f, 0.23f, 1.0f) :
		bHovered ? ImVec4(0.14f, 0.16f, 0.19f, 1.0f) :
				   ImVec4(0.09f, 0.10f, 0.12f, 1.0f));
	DrawList->AddRectFilled(Min, Max, BgColor, 3.0f);

	if (Icon)
	{
		const float Padding = std::max(4.0f, Size.x * 0.16f);
		DrawList->AddImage(
			reinterpret_cast<ImTextureID>(Icon),
			ImVec2(Min.x + Padding, Min.y + Padding),
			ImVec2(Min.x + Size.x - Padding, Min.y + Size.y - Padding));
	}
	else if (Id && Id[0] != '\0')
	{
		const char Fallback[2] = { Id[0], '\0' };
		const ImVec2 TextSize = ImGui::CalcTextSize(Fallback);
		DrawList->AddText(
			ImVec2(Min.x + (Size.x - TextSize.x) * 0.5f, Min.y + (Size.y - TextSize.y) * 0.5f),
			ImGui::GetColorU32(ImVec4(0.72f, 0.76f, 0.84f, 1.0f)),
			Fallback);
	}
	if (Label)
	{
		DrawList->AddText(
			ImVec2(Min.x + Size.x + LabelGap, Min.y + (ButtonSize.y - LabelSize.y) * 0.5f),
			ImGui::GetColorU32(ImVec4(0.94f, 0.95f, 0.98f, bEnabled ? 1.0f : 0.45f)),
			Label);
	}

	if (bHovered && Tooltip)
	{
		ImGui::SetTooltip("%s", Tooltip);
	}

	if (!bEnabled)
	{
		ImGui::EndDisabled();
	}
	ImGui::PopID();
	return bEnabled && bClicked;
}

void FParticleEditorViewerWidget::RenderEmitterPanel(FParticleEditorViewer* Viewer)
{
	UParticleSystem* ParticleSystem = Viewer->GetParticleSystem();
	DrawParticlePanelTitle("Emitters", "Modules");

	if (!ParticleSystem)
	{
		ImGui::TextDisabled("No particle system");
		return;
	}

	if (ImGui::IsWindowFocused() && Viewer->GetSelectedEmitterIndex() >= 0)
	{
		if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
		{
			const int32 Index = Viewer->GetSelectedEmitterIndex();
			Viewer->MoveEmitter(Index, Index - 1);
		}
		if (ImGui::IsKeyPressed(ImGuiKey_RightArrow))
		{
			const int32 Index = Viewer->GetSelectedEmitterIndex();
			Viewer->MoveEmitter(Index, Index + 1);
		}
	}

	const int32 EmitterCount = static_cast<int32>(ParticleSystem->Emitters.size());
	if (EmitterCount > 0 && ImGui::BeginTable("##ParticleEmitterColumns", EmitterCount, ImGuiTableFlags_SizingFixedFit))
	{
		for (int32 EmitterIndex = 0; EmitterIndex < EmitterCount; ++EmitterIndex)
		{
			ImGui::TableSetupColumn(("Emitter " + std::to_string(EmitterIndex)).c_str(), ImGuiTableColumnFlags_WidthFixed, EmitterNodeWidth + EmitterSeparatorGap);
		}
		ImGui::TableNextRow();
		for (int32 EmitterIndex = 0; EmitterIndex < EmitterCount; ++EmitterIndex)
		{
			ImGui::TableSetColumnIndex(EmitterIndex);
			ImGui::PushID(EmitterIndex);
			DrawEmitterNode(Viewer, EmitterIndex);
			ImGui::PopID();
		}
		ImGui::EndTable();
	}

	RenderEmitterContextMenu(Viewer);
}

void FParticleEditorViewerWidget::RenderEmitterContextMenu(FParticleEditorViewer* Viewer)
{
	if (ImGui::BeginPopupContextWindow("ParticleEmitterPanelContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
	{
		if (ImGui::MenuItem("Add Emitter"))
		{
			Viewer->AddEmitter();
		}
		if (Viewer->GetSelectedEmitterIndex() >= 0 && Viewer->GetSelectedLODLevel() != nullptr && ImGui::BeginMenu("Add Module"))
		{
			DrawParticleModuleClassMenu(Viewer);
			ImGui::EndMenu();
		}
		ImGui::EndPopup();
	}
}

void FParticleEditorViewerWidget::RenderDetailsPanel(FParticleEditorViewer* Viewer)
{
	UObject* SelectedObject = Viewer->GetSelectedObject();
	DrawParticlePanelTitle("Details", GetSelectionLabel(Viewer->GetSelectionType()));
	DrawParticleDetailsSection("Selection");
	DrawParticleDetailsText("Type", GetSelectionLabel(Viewer->GetSelectionType()));
	DrawParticleDetailsText("Object", GetObjectLabel(SelectedObject));

	if (!SelectedObject)
	{
		ImGui::Separator();
		ImGui::TextDisabled("Select a particle system, emitter, LOD, or module.");
		return;
	}

	ImGui::Separator();
	if (Viewer->GetSelectionType() == EParticleEditorSelectionType::ParticleSystem)
	{
		UParticleSystem* ParticleSystem = Viewer->GetParticleSystem();
		DrawParticleDetailsSection("Particle System");
		ImGui::Text("Emitter Count");
		ImGui::SameLine(150.0f);
		ImGui::Text("%d", ParticleSystem ? static_cast<int32>(ParticleSystem->Emitters.size()) : 0);
		return;
	}

	if (UParticleLODLevel* LODLevel = Cast<UParticleLODLevel>(SelectedObject))
	{
		DrawParticleDetailsSection("LOD");
		ImGui::PushItemWidth(-1.0f);
		if (ImGui::InputInt("Level", &LODLevel->Level))
		{
			Viewer->MarkDirty();
		}
		if (ImGui::Checkbox("Enabled", &LODLevel->bEnabled))
		{
			Viewer->MarkDirty();
			Viewer->RestartSimulation();
		}
		ImGui::PopItemWidth();
		return;
	}

	if (UParticleEmitter* Emitter = Cast<UParticleEmitter>(SelectedObject))
	{
		DrawParticleDetailsSection("Emitter");
		ImGui::Text("LOD Count");
		ImGui::SameLine(150.0f);
		ImGui::Text("%d", static_cast<int32>(Emitter->LODLevels.size()));
		ImGui::Text("Runtime Caches");
		ImGui::SameLine(150.0f);
		ImGui::Text("%d", static_cast<int32>(Emitter->LODLevelRuntimeCaches.size()));
		return;
	}

	if (UParticleModuleRequired* Required = Cast<UParticleModuleRequired>(SelectedObject))
	{
		DrawParticleDetailsSection("Required Module");
		ImGui::PushItemWidth(-1.0f);
		if (ImGui::InputInt("Max Particles", &Required->MaxParticles))
		{
			Required->MaxParticles = std::max(1, Required->MaxParticles);
			Viewer->MarkDirty();
			Viewer->RestartSimulation();
		}
		if (ImGui::InputFloat("Emitter Duration", &Required->EmitterDuration, 0.1f, 1.0f, "%.3f"))
		{
			Required->EmitterDuration = std::max(0.0f, Required->EmitterDuration);
			Viewer->MarkDirty();
			Viewer->RestartSimulation();
		}
		if (ImGui::Checkbox("Emitter Loops", &Required->bEmitterLoops))
		{
			Viewer->MarkDirty();
			Viewer->RestartSimulation();
		}
		ImGui::PopItemWidth();
		return;
	}

	if (UParticleModuleSpawn* Spawn = Cast<UParticleModuleSpawn>(SelectedObject))
	{
		DrawParticleDetailsSection("Spawn Module");
		ImGui::PushItemWidth(-1.0f);
		if (ImGui::InputFloat("Spawn Rate", &Spawn->SpawnRate, 1.0f, 10.0f, "%.3f"))
		{
			Spawn->SpawnRate = std::max(0.0f, Spawn->SpawnRate);
			Viewer->MarkDirty();
			Viewer->RestartSimulation();
		}
		if (ImGui::InputFloat("Rate Scale", &Spawn->RateScale, 0.1f, 1.0f, "%.3f"))
		{
			Spawn->RateScale = std::max(0.0f, Spawn->RateScale);
			Viewer->MarkDirty();
			Viewer->RestartSimulation();
		}
		if (ImGui::Checkbox("Process Spawn Rate", &Spawn->bProcessSpawnRate))
		{
			Viewer->MarkDirty();
			Viewer->RestartSimulation();
		}
		ImGui::PopItemWidth();
		return;
	}

	DrawParticleDetailsSection("Properties");
	ImGui::TextDisabled("No reflected editable properties are exposed for this module yet.");
}

void FParticleEditorViewerWidget::RenderCurveEditor(FParticleEditorViewer* Viewer)
{
	DrawParticlePanelTitle("Curve Editor", "Channels");
	if (ImGui::SmallButton("Remove Curve"))
	{
		CurveSourceModuleIndex = -1;
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("Remove All"))
	{
		CurveSourceModuleIndex = -1;
	}
	ImGui::Separator();

	UParticleLODLevel* LOD = Viewer->GetSelectedLODLevel();
	if (!LOD || CurveSourceModuleIndex < 0 || CurveSourceModuleIndex >= static_cast<int32>(LOD->Modules.size()))
	{
		ImGui::TextDisabled("Click a module curve icon in the emitter panel.");
		return;
	}

	UParticleModule* Module = LOD->Modules[CurveSourceModuleIndex];
	ImGui::Text("Source: %s", GetObjectLabel(Module));

	const ImVec2 CanvasPos = ImGui::GetCursorScreenPos();
	const ImVec2 CanvasSize = ImGui::GetContentRegionAvail();
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->AddRectFilled(CanvasPos, ImVec2(CanvasPos.x + CanvasSize.x, CanvasPos.y + CanvasSize.y), IM_COL32(24, 24, 28, 255));
	DrawList->AddRect(CanvasPos, ImVec2(CanvasPos.x + CanvasSize.x, CanvasPos.y + CanvasSize.y), IM_COL32(70, 70, 78, 255));
	for (int32 Line = 1; Line < 5; ++Line)
	{
		const float X = CanvasPos.x + CanvasSize.x * (static_cast<float>(Line) / 5.0f);
		const float Y = CanvasPos.y + CanvasSize.y * (static_cast<float>(Line) / 5.0f);
		DrawList->AddLine(ImVec2(X, CanvasPos.y), ImVec2(X, CanvasPos.y + CanvasSize.y), IM_COL32(45, 45, 50, 255));
		DrawList->AddLine(ImVec2(CanvasPos.x, Y), ImVec2(CanvasPos.x + CanvasSize.x, Y), IM_COL32(45, 45, 50, 255));
	}
	DrawList->AddText(ImVec2(CanvasPos.x + 12.0f, CanvasPos.y + 10.0f), IM_COL32(160, 170, 185, 255), "Curve channels will appear here");
	ImGui::Dummy(CanvasSize);
}

void FParticleEditorViewerWidget::DrawEmitterNode(FParticleEditorViewer* Viewer, int32 EmitterIndex)
{
	UParticleSystem* ParticleSystem = Viewer->GetParticleSystem();
	if (!ParticleSystem || EmitterIndex < 0 || EmitterIndex >= static_cast<int32>(ParticleSystem->Emitters.size()))
	{
		return;
	}

	UParticleEmitter* Emitter = ParticleSystem->Emitters[EmitterIndex];
	const int32 LODIndex = Viewer->GetSelectedEmitterIndex() == EmitterIndex && Viewer->GetSelectedLODIndex() >= 0
		? Viewer->GetSelectedLODIndex()
		: 0;
	UParticleLODLevel* LOD = Emitter && LODIndex >= 0 && LODIndex < static_cast<int32>(Emitter->LODLevels.size())
		? Emitter->LODLevels[LODIndex]
		: nullptr;
	const bool bSelected =
		Viewer->GetSelectedEmitterIndex() == EmitterIndex &&
		(Viewer->GetSelectionType() == EParticleEditorSelectionType::Emitter ||
		 Viewer->GetSelectionType() == EParticleEditorSelectionType::LODLevel ||
		 Viewer->GetSelectionType() == EParticleEditorSelectionType::RequiredModule ||
		 Viewer->GetSelectionType() == EParticleEditorSelectionType::SpawnModule ||
		 Viewer->GetSelectionType() == EParticleEditorSelectionType::TypeDataModule ||
		 Viewer->GetSelectionType() == EParticleEditorSelectionType::Module);

	const ImVec2 CardStart = ImGui::GetCursorScreenPos();
	const float CardWidth = EmitterNodeWidth;
	const float HeaderHeight = 86.0f;
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	const float SeparatorX = CardStart.x + CardWidth + EmitterSeparatorGap * 0.5f;
	const float SeparatorBottom = ImGui::GetWindowPos().y + ImGui::GetWindowHeight() - ImGui::GetStyle().WindowPadding.y;

	DrawList->AddLine(ImVec2(SeparatorX, CardStart.y), ImVec2(SeparatorX, SeparatorBottom), IM_COL32(58, 60, 68, 255), 1.0f);
	DrawList->AddRectFilled(CardStart, ImVec2(CardStart.x + CardWidth, CardStart.y + HeaderHeight), IM_COL32(33, 34, 38, 255), 4.0f);
	DrawList->AddRect(CardStart, ImVec2(CardStart.x + CardWidth, CardStart.y + HeaderHeight), IM_COL32(75, 75, 82, 255), 4.0f);

	ImGui::InvisibleButton("##EmitterHeader", ImVec2(CardWidth, HeaderHeight));
	if (ImGui::IsItemClicked())
	{
		Viewer->SelectEmitter(EmitterIndex);
	}
	if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
	{
		Viewer->SelectEmitter(EmitterIndex);
		Viewer->SelectLOD(LODIndex);
	}
	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(ParticleModuleDragPayload))
		{
			const FParticleModuleDragPayload* DragPayload = static_cast<const FParticleModuleDragPayload*>(Payload->Data);
			if (DragPayload)
			{
				Viewer->SelectEmitter(DragPayload->EmitterIndex);
				Viewer->SelectLOD(DragPayload->LODIndex);
				if (ImGui::GetIO().KeyCtrl)
				{
					Viewer->CopyModuleToEmitter(DragPayload->ModuleIndex, EmitterIndex);
				}
				else
				{
					Viewer->MoveModuleToEmitter(DragPayload->ModuleIndex, EmitterIndex);
				}
			}
		}
		if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(ParticleEmitterDragPayload))
		{
			const FParticleEmitterDragPayload* DragPayload = static_cast<const FParticleEmitterDragPayload*>(Payload->Data);
			if (DragPayload)
			{
				Viewer->MoveEmitter(DragPayload->EmitterIndex, EmitterIndex);
			}
		}
		ImGui::EndDragDropTarget();
	}
	if (ImGui::BeginPopupContextItem("EmitterHeaderContext"))
	{
		if (ImGui::MenuItem("Add Emitter"))
		{
			Viewer->AddEmitter();
		}
		if (LOD && ImGui::BeginMenu("Add Module"))
		{
			DrawParticleModuleClassMenu(Viewer);
			ImGui::EndMenu();
		}
		ImGui::EndPopup();
	}
	if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
	{
		FParticleEmitterDragPayload Payload = {
			EmitterIndex
		};
		ImGui::SetDragDropPayload(ParticleEmitterDragPayload, &Payload, sizeof(Payload));
		ImGui::Text("Emitter %d", EmitterIndex);
		ImGui::EndDragDropSource();
	}

	ImGui::SetCursorScreenPos(ImVec2(CardStart.x + 14.0f, CardStart.y + 10.0f));
	ImGui::Text("Emitter %d", EmitterIndex);
	ImGui::SetCursorScreenPos(ImVec2(CardStart.x + 14.0f, CardStart.y + 40.0f));
	bool bEnabled = LOD ? LOD->bEnabled : false;
	if (!LOD)
	{
		ImGui::BeginDisabled();
	}
	if (ImGui::Checkbox("##EmitterEnabled", &bEnabled) && LOD)
	{
		LOD->bEnabled = bEnabled;
		Viewer->SelectEmitter(EmitterIndex);
		Viewer->SelectLOD(LODIndex);
		Viewer->MarkDirty();
		Viewer->RestartSimulation();
	}
	if (!LOD)
	{
		ImGui::EndDisabled();
	}
	ImGui::SameLine(0.0f, 6.0f);
	ImGui::TextUnformatted("Enable");
	ImGui::SetCursorScreenPos(ImVec2(CardStart.x + CardWidth - 78.0f, CardStart.y + 11.0f));
	DrawEmitterPreview(ImVec2(64.0f, 64.0f), EmitterIndex, bSelected);
	ImGui::SetCursorScreenPos(ImVec2(CardStart.x, CardStart.y + HeaderHeight + 6.0f));

	if (!LOD)
	{
		ImGui::TextDisabled("No LOD");
		const ImVec2 CardEnd(CardStart.x + CardWidth, ImGui::GetCursorScreenPos().y);
		if (bSelected)
		{
			DrawList->AddRect(CardStart, CardEnd, IM_COL32(240, 219, 79, 255), 4.0f, 0, 2.0f);
		}
		return;
	}

	if (LOD->RequiredModule)
	{
		DrawSelectableModuleRow(
			Viewer,
			"Required Module",
			EParticleEditorSelectionType::RequiredModule,
			EmitterIndex,
			LODIndex,
			-1,
			IM_COL32(244, 232, 156, 62));
	}

	if (LOD->SpawnModule)
	{
		DrawSelectableModuleRow(
			Viewer,
			"Spawn Module",
			EParticleEditorSelectionType::SpawnModule,
			EmitterIndex,
			LODIndex,
			-1,
			IM_COL32(244, 150, 150, 58));
	}

	for (int32 ModuleIndex = 0; ModuleIndex < static_cast<int32>(LOD->Modules.size()); ++ModuleIndex)
	{
		UParticleModule* Module = LOD->Modules[ModuleIndex];
		const bool bSpawnModule = Cast<UParticleModuleSpawn>(Module) != nullptr;
		DrawSelectableModuleRow(
			Viewer,
			bSpawnModule ? "Spawn Module" : GetObjectLabel(Module),
			EParticleEditorSelectionType::Module,
			EmitterIndex,
			LODIndex,
			ModuleIndex,
			bSpawnModule ? IM_COL32(244, 150, 150, 58) : IM_COL32(0, 0, 0, 0));
	}

	if (LOD->TypeDataModule)
	{
		DrawSelectableModuleRow(
			Viewer,
			"Type Data Module",
			EParticleEditorSelectionType::TypeDataModule,
			EmitterIndex,
			LODIndex,
			-1,
			IM_COL32(150, 190, 244, 45));
	}

	const ImVec2 CardEnd(CardStart.x + CardWidth, ImGui::GetCursorScreenPos().y);
	if (bSelected)
	{
		DrawList->AddRect(CardStart, CardEnd, IM_COL32(240, 219, 79, 255), 4.0f, 0, 2.0f);
	}
}

void FParticleEditorViewerWidget::DrawLODNode(FParticleEditorViewer* Viewer, int32 EmitterIndex, int32 LODIndex)
{
	UParticleSystem* ParticleSystem = Viewer->GetParticleSystem();
	if (!ParticleSystem || EmitterIndex < 0 || EmitterIndex >= static_cast<int32>(ParticleSystem->Emitters.size()))
	{
		return;
	}

	UParticleEmitter* Emitter = ParticleSystem->Emitters[EmitterIndex];
	if (!Emitter || LODIndex < 0 || LODIndex >= static_cast<int32>(Emitter->LODLevels.size()))
	{
		return;
	}

	const bool bSelected = Viewer->GetSelectionType() == EParticleEditorSelectionType::LODLevel &&
		Viewer->GetSelectedEmitterIndex() == EmitterIndex &&
		Viewer->GetSelectedLODIndex() == LODIndex;
	ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (bSelected)
	{
		Flags |= ImGuiTreeNodeFlags_Selected;
	}

	const bool bOpen = ImGui::TreeNodeEx((void*)(intptr_t)LODIndex, Flags, "LOD %d", LODIndex);
	if (ImGui::IsItemClicked())
	{
		Viewer->SelectEmitter(EmitterIndex);
		Viewer->SelectLOD(LODIndex);
	}

	if (bOpen)
	{
		UParticleLODLevel* LOD = Emitter->LODLevels[LODIndex];
		if (LOD && LOD->RequiredModule)
		{
			ImGuiTreeNodeFlags RequiredFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
			if (Viewer->GetSelectionType() == EParticleEditorSelectionType::RequiredModule &&
				Viewer->GetSelectedEmitterIndex() == EmitterIndex &&
				Viewer->GetSelectedLODIndex() == LODIndex)
			{
				RequiredFlags |= ImGuiTreeNodeFlags_Selected;
			}
			ImGui::TreeNodeEx("Required", RequiredFlags, "Required");
			if (ImGui::IsItemClicked())
			{
				Viewer->SelectEmitter(EmitterIndex);
				Viewer->SelectLOD(LODIndex);
				Viewer->SelectRequiredModule();
			}
		}

		if (LOD && LOD->SpawnModule)
		{
			ImGuiTreeNodeFlags SpawnFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
			if (Viewer->GetSelectionType() == EParticleEditorSelectionType::SpawnModule &&
				Viewer->GetSelectedEmitterIndex() == EmitterIndex &&
				Viewer->GetSelectedLODIndex() == LODIndex)
			{
				SpawnFlags |= ImGuiTreeNodeFlags_Selected;
			}
			ImGui::TreeNodeEx("Spawn", SpawnFlags, "Spawn");
			if (ImGui::IsItemClicked())
			{
				Viewer->SelectEmitter(EmitterIndex);
				Viewer->SelectLOD(LODIndex);
				Viewer->SelectSpawnModule();
			}
		}

		if (LOD && LOD->TypeDataModule)
		{
			ImGuiTreeNodeFlags TypeDataFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
			if (Viewer->GetSelectionType() == EParticleEditorSelectionType::TypeDataModule &&
				Viewer->GetSelectedEmitterIndex() == EmitterIndex &&
				Viewer->GetSelectedLODIndex() == LODIndex)
			{
				TypeDataFlags |= ImGuiTreeNodeFlags_Selected;
			}
			ImGui::TreeNodeEx("TypeData", TypeDataFlags, "Type Data");
			if (ImGui::IsItemClicked())
			{
				Viewer->SelectEmitter(EmitterIndex);
				Viewer->SelectLOD(LODIndex);
				Viewer->SelectTypeDataModule();
			}
		}

		for (int32 ModuleIndex = 0; LOD && ModuleIndex < static_cast<int32>(LOD->Modules.size()); ++ModuleIndex)
		{
			DrawModuleNode(Viewer, EmitterIndex, LODIndex, ModuleIndex);
		}
		ImGui::TreePop();
	}
}

void FParticleEditorViewerWidget::DrawModuleNode(FParticleEditorViewer* Viewer, int32 EmitterIndex, int32 LODIndex, int32 ModuleIndex)
{
	UParticleSystem* ParticleSystem = Viewer->GetParticleSystem();
	if (!ParticleSystem || EmitterIndex < 0 || EmitterIndex >= static_cast<int32>(ParticleSystem->Emitters.size()))
	{
		return;
	}

	UParticleEmitter* Emitter = ParticleSystem->Emitters[EmitterIndex];
	if (!Emitter || LODIndex < 0 || LODIndex >= static_cast<int32>(Emitter->LODLevels.size()))
	{
		return;
	}

	UParticleLODLevel* LOD = Emitter->LODLevels[LODIndex];
	if (!LOD || ModuleIndex < 0 || ModuleIndex >= static_cast<int32>(LOD->Modules.size()))
	{
		return;
	}

	UParticleModule* Module = LOD->Modules[ModuleIndex];
	ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (Viewer->GetSelectionType() == EParticleEditorSelectionType::Module &&
		Viewer->GetSelectedEmitterIndex() == EmitterIndex &&
		Viewer->GetSelectedLODIndex() == LODIndex &&
		Viewer->GetSelectedModuleIndex() == ModuleIndex)
	{
		Flags |= ImGuiTreeNodeFlags_Selected;
	}

	ImGui::TreeNodeEx((void*)(intptr_t)ModuleIndex, Flags, "%s", GetObjectLabel(Module));
	if (ImGui::IsItemClicked())
	{
		Viewer->SelectEmitterModule(EmitterIndex, LODIndex, ModuleIndex);
	}
	if (ImGui::BeginDragDropSource())
	{
		FParticleModuleDragPayload Payload = {
			EmitterIndex,
			LODIndex,
			ModuleIndex
		};
		ImGui::SetDragDropPayload(ParticleModuleDragPayload, &Payload, sizeof(Payload));
		ImGui::Text("Module: %s", GetObjectLabel(Module));
		ImGui::EndDragDropSource();
	}
	if (ImGui::BeginPopupContextItem("ModuleContext"))
	{
		if (ImGui::MenuItem("Delete Module"))
		{
			Viewer->SelectEmitterModule(EmitterIndex, LODIndex, ModuleIndex);
			Viewer->DeleteSelectedModule();
		}
		if (ImGui::MenuItem("Move Up"))
		{
			Viewer->SelectEmitterModule(EmitterIndex, LODIndex, ModuleIndex);
			Viewer->MoveModule(ModuleIndex, ModuleIndex - 1);
		}
		if (ImGui::MenuItem("Move Down"))
		{
			Viewer->SelectEmitterModule(EmitterIndex, LODIndex, ModuleIndex);
			Viewer->MoveModule(ModuleIndex, ModuleIndex + 1);
		}
		ImGui::EndPopup();
	}

	ImGui::SameLine();
	if (ImGui::SmallButton(("C##Curve" + std::to_string(ModuleIndex)).c_str()))
	{
		CurveSourceModuleIndex = ModuleIndex;
		Viewer->SelectEmitterModule(EmitterIndex, LODIndex, ModuleIndex);
	}
}

namespace
{
	FParticleEditorViewer* AsParticleViewer(FEditorViewer* Viewer)
	{
		return Viewer && Viewer->GetTabKind() == EEditorTabKind::ParticleViewer
			? static_cast<FParticleEditorViewer*>(Viewer)
			: nullptr;
	}

	const char* GetSelectionLabel(EParticleEditorSelectionType Type)
	{
		switch (Type)
		{
		case EParticleEditorSelectionType::ParticleSystem: return "Particle System";
		case EParticleEditorSelectionType::Emitter: return "Emitter";
		case EParticleEditorSelectionType::LODLevel: return "LOD Level";
		case EParticleEditorSelectionType::RequiredModule: return "Required Module";
		case EParticleEditorSelectionType::SpawnModule: return "Spawn Module";
		case EParticleEditorSelectionType::TypeDataModule: return "Type Data Module";
		case EParticleEditorSelectionType::Module: return "Module";
		case EParticleEditorSelectionType::None:
		default: return "None";
		}
	}

	const char* GetObjectLabel(const UObject* Object)
	{
		return Object ? Object->GetClassName() : "None";
	}

	void DrawParticlePanelTitle(const char* Title, const char* Subtitle)
	{
		const ImVec2 Start = ImGui::GetCursorScreenPos();
		const float Width = ImGui::GetContentRegionAvail().x;
		const float Height = 34.0f;
		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		const ImVec2 End(Start.x + Width, Start.y + Height);

		DrawList->AddRectFilled(Start, End, IM_COL32(27, 29, 35, 255), 3.0f);
		DrawList->AddRectFilled(Start, ImVec2(Start.x + Width, Start.y + 2.0f), IM_COL32(112, 146, 214, 255), 3.0f);
		DrawList->AddLine(ImVec2(Start.x, End.y), End, IM_COL32(66, 70, 82, 255), 1.0f);

		const ImVec2 TitleSize = ImGui::CalcTextSize(Title);
		DrawList->AddText(
			ImVec2(Start.x + 10.0f, Start.y + (Height - TitleSize.y) * 0.5f),
			ImGui::GetColorU32(ImVec4(0.93f, 0.95f, 1.0f, 1.0f)),
			Title);

		if (Subtitle && Subtitle[0] != '\0')
		{
			const ImVec2 SubtitleSize = ImGui::CalcTextSize(Subtitle);
			DrawList->AddText(
				ImVec2(End.x - SubtitleSize.x - 10.0f, Start.y + (Height - SubtitleSize.y) * 0.5f),
				ImGui::GetColorU32(ImVec4(0.56f, 0.60f, 0.68f, 1.0f)),
				Subtitle);
		}

		ImGui::Dummy(ImVec2(Width, Height + 8.0f));
	}

	void DrawParticleDetailsSection(const char* Title)
	{
		ImGui::Spacing();
		ImGui::TextColored(ImVec4(0.72f, 0.77f, 0.88f, 1.0f), "%s", Title);
		ImGui::Separator();
	}

	void DrawParticleDetailsText(const char* Label, const char* Value)
	{
		ImGui::TextDisabled("%s", Label);
		ImGui::SameLine(150.0f);
		ImGui::TextUnformatted(Value ? Value : "");
	}

	void GetParticleModuleClasses(TArray<UClass*>& OutClasses)
	{
		OutClasses.clear();
		FReflectionRegistry::Get().GetClassesDerivedFrom(UParticleModule::StaticClass(), OutClasses);
		OutClasses.erase(
			std::remove_if(
				OutClasses.begin(),
				OutClasses.end(),
				[](const UClass* Class)
				{
					return !Class ||
						Class == UParticleModule::StaticClass() ||
						Class->HasAnyClassFlags(CF_Abstract);
				}),
			OutClasses.end());
		std::stable_sort(
			OutClasses.begin(),
			OutClasses.end(),
			[](const UClass* Lhs, const UClass* Rhs)
			{
				return std::strcmp(Lhs->GetDisplayName(), Rhs->GetDisplayName()) < 0;
			});
	}

	bool DrawParticleModuleClassMenu(FParticleEditorViewer* Viewer)
	{
		TArray<UClass*> ModuleClasses;
		GetParticleModuleClasses(ModuleClasses);

		if (ModuleClasses.empty())
		{
			ImGui::TextDisabled("No particle module classes");
			return false;
		}

		bool bAdded = false;
		for (UClass* ModuleClass : ModuleClasses)
		{
			if (!ModuleClass)
			{
				continue;
			}

			if (ImGui::MenuItem(ModuleClass->GetDisplayName()))
			{
				Viewer->AddModule(ModuleClass);
				bAdded = true;
			}
		}
		return bAdded;
	}

	bool DrawPopupButton(const char* Label, const char* PopupId)
	{
		if (ImGui::Button(Label))
		{
			ImGui::OpenPopup(PopupId);
		}
		return true;
	}

	bool DrawRoundedToolbarButton(const char* Id, const char* Label, const char* Tooltip, const ImVec2& Size)
	{
		ImGui::PushID(Id);
		const bool bPressed = ImGui::InvisibleButton("##RoundedToolbarButton", Size);
		const bool bHovered = ImGui::IsItemHovered();
		const bool bActive = ImGui::IsItemActive();
		const ImVec2 Min = ImGui::GetItemRectMin();
		const ImVec2 Max = ImGui::GetItemRectMax();
		const ImVec2 Center((Min.x + Max.x) * 0.5f, (Min.y + Max.y) * 0.5f);
		ImDrawList* DrawList = ImGui::GetWindowDrawList();

		const ImU32 FillColor = bActive ? IM_COL32(56, 104, 174, 232) : (bHovered ? IM_COL32(52, 58, 70, 220) : IM_COL32(28, 31, 38, 190));
		const ImU32 BorderColor = bHovered ? IM_COL32(190, 205, 238, 255) : IM_COL32(112, 120, 138, 230);
		DrawList->AddRectFilled(Min, Max, FillColor, 8.0f);
		DrawList->AddRect(Min, Max, BorderColor, 8.0f, 0, 1.0f);

		const ImVec2 TextSize = ImGui::CalcTextSize(Label);
		DrawList->AddText(ImVec2(Center.x - TextSize.x * 0.5f, Center.y - TextSize.y * 0.5f), ImGui::GetColorU32(ImGuiCol_Text), Label);

		if (bHovered && Tooltip && Tooltip[0] != '\0')
		{
			ImGui::SetTooltip("%s", Tooltip);
		}
		ImGui::PopID();
		return bPressed;
	}

	void DrawEmitterPreview(const ImVec2& Size, int32 EmitterIndex, bool bSelected)
	{
		const ImVec2 Start = ImGui::GetCursorScreenPos();
		ImGui::InvisibleButton("##EmitterPreview", Size);

		const ImVec2 End(Start.x + Size.x, Start.y + Size.y);
		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		DrawList->AddRectFilled(Start, End, IM_COL32(18, 20, 24, 255), 4.0f);
		DrawList->AddRect(Start, End, bSelected ? IM_COL32(240, 219, 79, 255) : IM_COL32(76, 78, 86, 255), 4.0f);

		const ImU32 Warm = IM_COL32(255, 126, 82, 210);
		const ImU32 Cool = IM_COL32(104, 190, 255, 180);
		const ImU32 Soft = IM_COL32(255, 220, 130, 170);
		for (int32 Index = 0; Index < 9; ++Index)
		{
			const float XRatio = 0.18f + 0.68f * static_cast<float>(((EmitterIndex * 17 + Index * 31) % 100)) / 100.0f;
			const float YRatio = 0.18f + 0.62f * static_cast<float>(((EmitterIndex * 29 + Index * 23) % 100)) / 100.0f;
			const float Radius = 2.0f + static_cast<float>((Index + EmitterIndex) % 4);
			const ImU32 Color = Index % 3 == 0 ? Warm : (Index % 3 == 1 ? Cool : Soft);
			const ImVec2 Center(Start.x + Size.x * XRatio, Start.y + Size.y * YRatio);
			DrawList->AddCircleFilled(Center, Radius, Color, 12);
		}
		DrawList->AddLine(
			ImVec2(Start.x + Size.x * 0.16f, End.y - Size.y * 0.18f),
			ImVec2(End.x - Size.x * 0.14f, Start.y + Size.y * 0.24f),
			IM_COL32(255, 255, 255, 32),
			1.0f);
	}

	void DrawSelectableModuleRow(
		FParticleEditorViewer* Viewer,
		const char* Label,
		EParticleEditorSelectionType Type,
		int32 EmitterIndex,
		int32 LODIndex,
		int32 ModuleIndex,
		ImU32 BackgroundColor)
	{
		const bool bSelected =
			Viewer->GetSelectionType() == Type &&
			Viewer->GetSelectedEmitterIndex() == EmitterIndex &&
			Viewer->GetSelectedLODIndex() == LODIndex &&
			(Type != EParticleEditorSelectionType::Module || Viewer->GetSelectedModuleIndex() == ModuleIndex);

		ImGui::PushID(static_cast<int>(Type));
		ImGui::PushID(ModuleIndex);

		const ImVec2 RowStart = ImGui::GetCursorScreenPos();
		const float RowHeight = ImGui::GetTextLineHeight() + 6.0f;
		const float TextLeftPadding = 12.0f;
		const float RowWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x - EmitterSeparatorGap);
		const ImVec2 RowEnd(RowStart.x + RowWidth, RowStart.y + RowHeight);
		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		if ((BackgroundColor & IM_COL32_A_MASK) != 0)
		{
			DrawList->AddRectFilled(
				RowStart,
				RowEnd,
				BackgroundColor,
				3.0f);
		}

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
		const bool bPressed = ImGui::InvisibleButton("##SelectableModuleRow", ImVec2(RowWidth, RowHeight));
		ImGui::PopStyleVar();

		if (bSelected || ImGui::IsItemHovered())
		{
			const ImU32 StateColor = ImGui::GetColorU32(
				bSelected
					? ImVec4(0.22f, 0.33f, 0.55f, 0.78f)
					: ImVec4(0.22f, 0.24f, 0.30f, 0.42f));
			DrawList->AddRectFilled(RowStart, RowEnd, StateColor, 3.0f);
		}

		const ImVec2 TextSize = ImGui::CalcTextSize(Label);
		DrawList->AddText(
			ImVec2(RowStart.x + TextLeftPadding, RowStart.y + (RowHeight - TextSize.y) * 0.5f),
			ImGui::GetColorU32(ImGuiCol_Text),
			Label);

		if (bPressed)
		{
			Viewer->SelectEmitter(EmitterIndex);
			Viewer->SelectLOD(LODIndex);
			switch (Type)
			{
			case EParticleEditorSelectionType::RequiredModule:
				Viewer->SelectRequiredModule();
				break;
			case EParticleEditorSelectionType::SpawnModule:
				Viewer->SelectSpawnModule();
				break;
			case EParticleEditorSelectionType::TypeDataModule:
				Viewer->SelectTypeDataModule();
				break;
			case EParticleEditorSelectionType::Module:
				Viewer->SelectEmitterModule(EmitterIndex, LODIndex, ModuleIndex);
				break;
			case EParticleEditorSelectionType::LODLevel:
				break;
			case EParticleEditorSelectionType::Emitter:
			case EParticleEditorSelectionType::ParticleSystem:
			case EParticleEditorSelectionType::None:
			default:
				break;
			}
		}

		if (Type == EParticleEditorSelectionType::Module)
		{
			if (ImGui::BeginDragDropSource())
			{
				FParticleModuleDragPayload Payload = {
					EmitterIndex,
					LODIndex,
					ModuleIndex
				};
				ImGui::SetDragDropPayload(ParticleModuleDragPayload, &Payload, sizeof(Payload));
				ImGui::Text("Module: %s", Label);
				ImGui::EndDragDropSource();
			}

			if (ImGui::BeginPopupContextItem("ModuleContext"))
			{
				if (ImGui::MenuItem("Delete Module"))
				{
					Viewer->SelectEmitterModule(EmitterIndex, LODIndex, ModuleIndex);
					Viewer->DeleteSelectedModule();
				}
				if (ImGui::MenuItem("Move Up"))
				{
					Viewer->SelectEmitterModule(EmitterIndex, LODIndex, ModuleIndex);
					Viewer->MoveModule(ModuleIndex, ModuleIndex - 1);
				}
				if (ImGui::MenuItem("Move Down"))
				{
					Viewer->SelectEmitterModule(EmitterIndex, LODIndex, ModuleIndex);
					Viewer->MoveModule(ModuleIndex, ModuleIndex + 1);
				}
				ImGui::EndPopup();
			}
		}
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(ParticleModuleDragPayload))
			{
				const FParticleModuleDragPayload* DragPayload = static_cast<const FParticleModuleDragPayload*>(Payload->Data);
				if (DragPayload)
				{
					const bool bSameModuleList =
						Type == EParticleEditorSelectionType::Module &&
						DragPayload->EmitterIndex == EmitterIndex &&
						DragPayload->LODIndex == LODIndex;
					if (bSameModuleList && !ImGui::GetIO().KeyCtrl)
					{
						Viewer->SelectEmitterModule(EmitterIndex, LODIndex, DragPayload->ModuleIndex);
						Viewer->MoveModule(DragPayload->ModuleIndex, ModuleIndex);
					}
					else
					{
						Viewer->SelectEmitter(DragPayload->EmitterIndex);
						Viewer->SelectLOD(DragPayload->LODIndex);
						if (ImGui::GetIO().KeyCtrl)
						{
							Viewer->CopyModuleToEmitter(DragPayload->ModuleIndex, EmitterIndex);
						}
						else
						{
							Viewer->MoveModuleToEmitter(DragPayload->ModuleIndex, EmitterIndex);
						}
					}
				}
			}
			ImGui::EndDragDropTarget();
		}

		ImGui::PopID();
		ImGui::PopID();
	}

	void DrawViewModeMenuItems(FParticleEditorViewer* Viewer)
	{
		FEditorMainPanelViewportToolbarHelpers::ForEachViewMode(
			[Viewer](EViewMode Mode)
			{
				if (ImGui::MenuItem(
					FEditorMainPanelViewportToolbarHelpers::GetViewModeName(Mode),
					nullptr,
					Viewer->GetViewMode() == Mode))
				{
					Viewer->SetViewMode(Mode);
				}
			});
	}
}
