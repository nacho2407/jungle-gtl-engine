#include "ParticleEditorViewerWidget.h"

#include "Core/Reflection/ReflectionRegistry.h"
#include "Core/Paths.h"
#include "Core/ResourceManager.h"
#include "Editor/Asset/EditorAssetService.h"
#include "Editor/EditorEngine.h"
#include "Editor/UI/EditorMainPanel.h"
#include "Editor/UI/EditorMainPanelViewportToolbarHelpers.h"
#include "Editor/Viewer/EditorViewer.h"
#include "Editor/Viewer/ParticleEditorViewer.h"
#include "Editor/Viewport/FSceneViewport.h"
#include "Engine/Core/EditorResourcePaths.h"
#include "Object/Class.h"
#include "Object/Property.h"
#include "Particle/ParticleAsset.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "WICTextureLoader.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <Windows.h>
#include <commdlg.h>

namespace
{
	constexpr const char* ParticleModuleDragPayload = "ParticleModule";
	constexpr const char* ParticleEmitterDragPayload = "ParticleEmitter";
	constexpr float EmitterNodeWidth = 198.0f;
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
	HWND ResolveSaveDialogOwnerWindow(const UEditorEngine* EditorEngine);
	bool OpenParticleSaveFileDialog(HWND OwnerWindow, const FParticleEditorViewer* Viewer, FString& OutFilePath);
	const char* GetSelectionLabel(EParticleEditorSelectionType Type);
	const char* GetObjectLabel(const UObject* Object);
	bool HasDeletableSelectedEmitter(FParticleEditorViewer* Viewer);
	bool HasDeletableSelectedModule(FParticleEditorViewer* Viewer);
	bool IsAnyPopupOpen();
	void HandleParticleEditorShortcuts(FParticleEditorViewer* Viewer);
	void GetParticleModuleClasses(TArray<UClass*>& OutClasses);
	bool DrawParticleModuleClassMenu(FParticleEditorViewer* Viewer);
	void DrawViewModeMenuItems(FParticleEditorViewer* Viewer);
	bool DrawPopupButton(const char* Label, const char* PopupId);
	bool DrawRoundedToolbarButton(const char* Id, const char* Label, const char* Tooltip, const ImVec2& Size);
	bool DrawCascadeGraphButton(const char* Id, const ImVec2& Size, bool bActive);
	bool DrawCurrentLODToolbarInput(FParticleEditorViewer* Viewer, ID3D11ShaderResourceView* Icon, const ImVec2& IconSize, const ImVec2& Size);
	void DrawParticlePanelTitle(const char* Title, const char* Subtitle);
	void DrawParticleDetailsSection(const char* Title);
	void DrawParticleDetailsText(const char* Label, const char* Value);

	// Property Helpers
	const char* GetPropertyDisplayName(const FProperty& Property);
	FString MakeParticlePropertyWidgetLabel(const FProperty& Property);
	bool IsParticleGraphReferenceProperty(const FProperty& Property);
	void CollectParticleEditableProperties(UObject* Object, TArray<const FProperty*>& OutProperties);
	bool RenderParticleReflectionProperties(FParticleEditorViewer* Viewer, UObject* Object, UEditorEngine* EditorEngine, bool& bUndoCaptured);
	bool RenderParticlePropertyWidget(FParticleEditorViewer* Viewer, UObject* Object, const FProperty& Property, UEditorEngine* EditorEngine, bool& bUndoCaptured);
	bool RenderParticlePropertyValueWidget(FParticleEditorViewer* Viewer, UObject* Object, const FProperty& Property, void* ValuePtr, const char* Label, UEditorEngine* EditorEngine, bool& bUndoCaptured);
	bool RenderParticleObjectPtrWidget(const FProperty& Property, void* ValuePtr, const char* Label, UEditorEngine* EditorEngine);
	bool RenderParticleSoftObjectPtrWidget(const FProperty& Property, void* ValuePtr, const char* Label, UEditorEngine* EditorEngine);
	bool RenderParticleArrayPropertyWidget(FParticleEditorViewer* Viewer, UObject* Object, const FProperty& Property, void* ValuePtr, UEditorEngine* EditorEngine, bool& bUndoCaptured);
	bool RenderParticleStructPropertyWidget(FParticleEditorViewer* Viewer, UObject* Object, const FProperty& Property, void* ValuePtr, const char* Label, UEditorEngine* EditorEngine, bool& bUndoCaptured);
	
	UParticleLODLevel* ResolveParticleLOD(FParticleEditorViewer* Viewer, int32 EmitterIndex, int32 LODIndex);
	UParticleModule* ResolveParticleModule(FParticleEditorViewer* Viewer, EParticleEditorSelectionType Type, int32 EmitterIndex, int32 LODIndex, int32 ModuleIndex);
	void SelectParticleModuleTarget(FParticleEditorViewer* Viewer, EParticleEditorSelectionType Type, int32 EmitterIndex, int32 LODIndex, int32 ModuleIndex);
	void DrawEmitterPreview(const ImVec2& Size, int32 EmitterIndex, bool bSelected);
	void HandleModuleDragDropTarget(FParticleEditorViewer* Viewer, EParticleEditorSelectionType Type, int32 EmitterIndex, int32 LODIndex, int32 ModuleIndex);
	int32 GetSelectedEmitterLODCount(FParticleEditorViewer* Viewer);
	bool IsUnsignedIntegerText(const char* Text);
	int FilterUnsignedIntegerInput(ImGuiInputTextCallbackData* Data);

	void DrawSelectableModuleRow(
		FParticleEditorViewer* Viewer, const char* Label,
		EParticleEditorSelectionType Type, int32 EmitterIndex, int32 LODIndex, int32 ModuleIndex, ImU32 BackgroundColor,
		EParticleEditorSelectionType& CurveSourceType, int32& CurveSourceEmitterIndex, int32& CurveSourceLODIndex, int32& CurveSourceModuleIndex);
} // namespace

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
	HandleParticleEditorShortcuts(ParticleViewer);

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
			if (ImGui::MenuItem("Save As", "Ctrl+S", false, Viewer->GetParticleSystem() != nullptr))
			{
				FString SavePath;
				if (OpenParticleSaveFileDialog(ResolveSaveDialogOwnerWindow(EditorEngine), Viewer, SavePath))
				{
					Viewer->SaveAs(SavePath);
				}
			}
			ImGui::EndPopup();
		}
	}
	ImGui::SameLine();
	if (DrawPopupButton("Edit", "##ParticleEditMenu"))
	{
		if (ImGui::BeginPopup("##ParticleEditMenu"))
		{
			if (ImGui::MenuItem("Undo", "Ctrl+Z", false, Viewer->CanUndo()))
			{
				Viewer->Undo();
			}
			if (ImGui::MenuItem("Redo", "Ctrl+Shift+Z", false, Viewer->CanRedo()))
			{
				Viewer->Redo();
			}
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
	auto DrawCurrentLODInput = [&]()
	{
		const float Width =
			IconSize.x +
			6.0f +
			ImGui::CalcTextSize("LOD:").x +
			4.0f +
			36.0f +
			8.0f;
		if (!CanFit(Width))
		{
			bHasOverflow = true;
			bHiddenCurrentLOD = true;
			return;
		}

		bHiddenCurrentLOD = false;
		DrawCurrentLODToolbarInput(Viewer, CascadeGenericLODIcon.Get(), IconSize, ImVec2(Width, IconSize.y));
		ImGui::SameLine();
	};

	DrawVisibleButton("Save", CascadeSaveIcon.Get(), "Save As", Viewer->GetParticleSystem() != nullptr, nullptr, bHiddenSave, [&]()
					  {
						  FString SavePath;
						  if (OpenParticleSaveFileDialog(ResolveSaveDialogOwnerWindow(EditorEngine), Viewer, SavePath))
						  {
							  Viewer->SaveAs(SavePath);
						  }
					  });
	DrawVisibleButton("Find", CascadeFindIcon.Get(), "Find in Content Browser", true, nullptr, bHiddenFind, [&]()
					  { Viewer->FindInContentBrowser(); });
	DrawSeparatorIfFits();
	DrawVisibleButton("RestartSim", CascadeRestartSimIcon.Get(), "Restart Simulation", true, "Restart Sim", bHiddenRestartSim, [&]()
					  { Viewer->RestartSimulation(); });
	DrawVisibleButton("RestartLevel", CascadeRestartLevelIcon.Get(), "Restart Level", true, "Restart Level", bHiddenRestartLevel, [&]()
					  { Viewer->RestartLevel(); });
	DrawSeparatorIfFits();
	DrawVisibleButton("Undo", CascadeUndoIcon.Get(), "Undo", Viewer->CanUndo(), "Undo", bHiddenUndo, [&]()
					  { Viewer->Undo(); });
	DrawVisibleButton("Redo", CascadeRedoIcon.Get(), "Redo", Viewer->CanRedo(), "Redo", bHiddenRedo, [&]()
					  { Viewer->Redo(); });
	DrawSeparatorIfFits();
	DrawVisibleButton("Thumbnail", CascadeThumbnailIcon.Get(), "Thumbnail", false, "Thumbnail", bHiddenThumbnail, []() {});
	DrawSeparatorIfFits();
	DrawVisibleButton("Bounds", CascadeBoundsIcon.Get(), Viewer->IsShowBounds() ? "Hide Bounds" : "Show Bounds", true, "Bounds", bHiddenBounds, [&]()
					  { Viewer->SetShowBounds(!Viewer->IsShowBounds()); });
	DrawVisibleButton("Axis", CascadeAxisIcon.Get(), Viewer->IsShowGrid() && Viewer->IsShowAxis() ? "Hide Axis/Grid" : "Show Axis/Grid", true, "Axis", bHiddenAxis, [&]()
					  {
						  const bool bNextVisible = !(Viewer->IsShowGrid() && Viewer->IsShowAxis());
						  Viewer->SetShowGrid(bNextVisible);
						  Viewer->SetShowAxis(bNextVisible);
					  });
	DrawVisibleButton("Background", CascadeBackgroundIcon.Get(), "Background", true, "Background", bHiddenBackground, [&]()
					  { bOpenBackgroundPopup = true; });
	DrawSeparatorIfFits();
	DrawVisibleButton("RegenLOD", CascadeRegenLODIcon.Get(), "Regenerate LOD", false, "Regen LOD", bHiddenRegenLOD, []() {});
	DrawVisibleButton("LowestLOD", CascadeLowestLODIcon.Get(), "Lowest LOD", true, "Lowest LOD", bHiddenLowestLOD, [&]()
					  { Viewer->SetLowestLOD(); });
	DrawVisibleButton("LowerLOD", CascadeLowerLODIcon.Get(), "Lower LOD", true, "Lower LOD", bHiddenLowerLOD, [&]()
					  { Viewer->SelectLowerLOD(); });
	DrawVisibleButton("AddLOD", CascadeAddLODIcon.Get(), "Add LOD", true, "Add LOD", bHiddenAddLOD, [&]()
					  { Viewer->AddLOD(); });
	DrawCurrentLODInput();
	DrawVisibleButton("UpperLOD", CascadeUpperLODIcon.Get(), "Upper LOD", true, "Upper LOD", bHiddenUpperLOD, [&]()
					  { Viewer->SelectUpperLOD(); });
	DrawVisibleButton("HighestLOD", CascadeHighestLODIcon.Get(), "Highest LOD", true, "Highest LOD", bHiddenHighestLOD, [&]()
					  { Viewer->SetHighestLOD(); });

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
			DrawOverflowButton("OverflowSave", CascadeSaveIcon.Get(), "Save As", Viewer->GetParticleSystem() != nullptr, "Save As", [&]()
							   {
								   FString SavePath;
								   if (OpenParticleSaveFileDialog(ResolveSaveDialogOwnerWindow(EditorEngine), Viewer, SavePath))
								   {
									   Viewer->SaveAs(SavePath);
								   }
							   });
			bNeedsSeparator = true;
		}
		if (bHiddenFind)
		{
			DrawOverflowButton("OverflowFind", CascadeFindIcon.Get(), "Find in Content Browser", true, "Find", [&]()
							   { Viewer->FindInContentBrowser(); });
			bNeedsSeparator = true;
		}
		DrawSeparatorForHiddenGroup(bNeedsSeparator && (bHiddenRestartSim || bHiddenRestartLevel || bHiddenUndo || bHiddenRedo || bHiddenThumbnail || bHiddenBounds || bHiddenAxis || bHiddenBackground || bHiddenRegenLOD || bHiddenLowestLOD || bHiddenLowerLOD || bHiddenAddLOD || bHiddenCurrentLOD || bHiddenUpperLOD || bHiddenHighestLOD));

		bNeedsSeparator = false;
		if (bHiddenRestartSim)
		{
			DrawOverflowButton("OverflowRestartSim", CascadeRestartSimIcon.Get(), "Restart Simulation", true, "Restart Sim", [&]()
							   { Viewer->RestartSimulation(); });
			bNeedsSeparator = true;
		}
		if (bHiddenRestartLevel)
		{
			DrawOverflowButton("OverflowRestartLevel", CascadeRestartLevelIcon.Get(), "Restart Level", true, "Restart Level", [&]()
							   { Viewer->RestartLevel(); });
			bNeedsSeparator = true;
		}
		DrawSeparatorForHiddenGroup(bNeedsSeparator && (bHiddenUndo || bHiddenRedo || bHiddenThumbnail || bHiddenBounds || bHiddenAxis || bHiddenBackground || bHiddenRegenLOD || bHiddenLowestLOD || bHiddenLowerLOD || bHiddenAddLOD || bHiddenCurrentLOD || bHiddenUpperLOD || bHiddenHighestLOD));

		bNeedsSeparator = false;
		if (bHiddenUndo)
		{
			DrawOverflowButton("OverflowUndo", CascadeUndoIcon.Get(), "Undo", Viewer->CanUndo(), "Undo", [&]()
							   { Viewer->Undo(); });
			bNeedsSeparator = true;
		}
		if (bHiddenRedo)
		{
			DrawOverflowButton("OverflowRedo", CascadeRedoIcon.Get(), "Redo", Viewer->CanRedo(), "Redo", [&]()
							   { Viewer->Redo(); });
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
			DrawOverflowButton("OverflowBounds", CascadeBoundsIcon.Get(), Viewer->IsShowBounds() ? "Hide Bounds" : "Show Bounds", true, "Bounds", [&]()
							   { Viewer->SetShowBounds(!Viewer->IsShowBounds()); });
			bNeedsSeparator = true;
		}
		if (bHiddenAxis)
		{
			DrawOverflowButton("OverflowAxis", CascadeAxisIcon.Get(), Viewer->IsShowGrid() && Viewer->IsShowAxis() ? "Hide Axis/Grid" : "Show Axis/Grid", true, "Axis", [&]()
							   {
								   const bool bNextVisible = !(Viewer->IsShowGrid() && Viewer->IsShowAxis());
								   Viewer->SetShowGrid(bNextVisible);
								   Viewer->SetShowAxis(bNextVisible);
							   });
			bNeedsSeparator = true;
		}
		if (bHiddenBackground)
		{
			DrawOverflowButton("OverflowBackground", CascadeBackgroundIcon.Get(), "Background", true, "Background", [&]()
							   { bOpenBackgroundPopup = true; });
			bNeedsSeparator = true;
		}
		DrawSeparatorForHiddenGroup(bNeedsSeparator && (bHiddenRegenLOD || bHiddenLowestLOD || bHiddenLowerLOD || bHiddenAddLOD || bHiddenCurrentLOD || bHiddenUpperLOD || bHiddenHighestLOD));

		if (bHiddenRegenLOD)
		{
			DrawOverflowButton("OverflowRegenLOD", CascadeRegenLODIcon.Get(), "Regenerate LOD", false, "Regen LOD", []() {});
		}
		if (bHiddenLowestLOD)
		{
			DrawOverflowButton("OverflowLowestLOD", CascadeLowestLODIcon.Get(), "Lowest LOD", true, "Lowest LOD", [&]()
							   { Viewer->SetLowestLOD(); });
		}
		if (bHiddenLowerLOD)
		{
			DrawOverflowButton("OverflowLowerLOD", CascadeLowerLODIcon.Get(), "Lower LOD", true, "Lower LOD", [&]()
							   { Viewer->SelectLowerLOD(); });
		}
		if (bHiddenAddLOD)
		{
			DrawOverflowButton("OverflowAddLOD", CascadeAddLODIcon.Get(), "Add LOD", true, "Add LOD", [&]()
							   { Viewer->AddLOD(); });
		}
		if (bHiddenCurrentLOD)
		{
			DrawCurrentLODToolbarInput(Viewer, CascadeGenericLODIcon.Get(), ImVec2(22.0f, 22.0f), ImVec2(94.0f, 24.0f));
		}
		if (bHiddenUpperLOD)
		{
			DrawOverflowButton("OverflowUpperLOD", CascadeUpperLODIcon.Get(), "Upper LOD", true, "Upper LOD", [&]()
							   { Viewer->SelectUpperLOD(); });
		}
		if (bHiddenHighestLOD)
		{
			DrawOverflowButton("OverflowHighestLOD", CascadeHighestLODIcon.Get(), "Highest LOD", true, "Highest LOD", [&]()
							   { Viewer->SetHighestLOD(); });
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
		bActive ? ImVec4(0.18f, 0.20f, 0.23f, 1.0f) : bHovered ? ImVec4(0.14f, 0.16f, 0.19f, 1.0f)
															   : ImVec4(0.09f, 0.10f, 0.12f, 1.0f));
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
	if (EmitterCount > 0)
	{
		ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0.0f, ImGui::GetStyle().CellPadding.y));
		constexpr ImGuiTableFlags TableFlags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollX;

		if (ImGui::BeginTable("##ParticleEmitterColumns", EmitterCount, TableFlags))
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
		ImGui::PopStyleVar();
	}

	// 패널에 남은 빈 공간 전체를 덮는 투명 버튼 생성
	const ImVec2 Avail = ImGui::GetContentRegionAvail();
	if (Avail.x > 0.0f && Avail.y > 0.0f)
	{
		ImGui::InvisibleButton("##EmitterPanelEmptySpace", Avail);
		if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
		{
			ImGui::OpenPopup("ParticleEmitterPanelContext");
		}
	}

	if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)
		&& ImGui::IsMouseClicked(ImGuiMouseButton_Right))
	{
		ImGui::OpenPopup("ParticleEmitterPanelContext");
	}

	RenderEmitterContextMenu(Viewer);
}

void FParticleEditorViewerWidget::RenderEmitterContextMenu(FParticleEditorViewer* Viewer)
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 6.0f));

	if (ImGui::BeginPopup("ParticleEmitterPanelContext"))
	{
		bool bDeletedEmitter = false;
		bool bDeletedModule = false;
		if (ImGui::MenuItem("Delete Emitter", nullptr, false, HasDeletableSelectedEmitter(Viewer)))
		{
			Viewer->DeleteSelectedEmitter();
			bDeletedEmitter = true;
		}
		if (!bDeletedEmitter && ImGui::MenuItem("Delete Module", nullptr, false, HasDeletableSelectedModule(Viewer)))
		{
			Viewer->DeleteSelectedModule();
			bDeletedModule = true;
		}
		if (!bDeletedEmitter && !bDeletedModule && ImGui::MenuItem("Add Emitter"))
		{
			Viewer->AddEmitter();
		}
		if (!bDeletedEmitter && !bDeletedModule && Viewer->GetSelectedEmitterIndex() >= 0 && Viewer->GetSelectedLODLevel() != nullptr && ImGui::BeginMenu("Add Module"))
		{
			DrawParticleModuleClassMenu(Viewer);
			ImGui::EndMenu();
		}
		ImGui::EndPopup();
	}

	ImGui::PopStyleVar(2);
}

void FParticleEditorViewerWidget::RenderDetailsPanel(FParticleEditorViewer* Viewer)
{
	UObject* SelectedObject = Viewer->GetSelectedObject();
	const char* SelectedTypeLabel = SelectedObject ? GetObjectLabel(SelectedObject) : GetSelectionLabel(Viewer->GetSelectionType());
	DrawParticlePanelTitle("Details", SelectedTypeLabel);
	DrawParticleDetailsSection("Selection");
	DrawParticleDetailsText("Type", SelectedTypeLabel);
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
	}
	else if (UParticleEmitter* Emitter = Cast<UParticleEmitter>(SelectedObject))
	{
		DrawParticleDetailsSection("Emitter");
		ImGui::Text("LOD Count");
		ImGui::SameLine(150.0f);
		ImGui::Text("%d", static_cast<int32>(Emitter->LODLevels.size()));
		ImGui::Text("Runtime Caches");
		ImGui::SameLine(150.0f);
		ImGui::Text("%d", static_cast<int32>(Emitter->LODLevelRuntimeCaches.size()));
	}

	DrawParticleDetailsSection("Properties");
	if (!RenderParticleReflectionProperties(Viewer, SelectedObject, EditorEngine, bPropertyEditUndoCaptured))
	{
		ImGui::TextDisabled("No reflected editable properties.");
	}
}


void FParticleEditorViewerWidget::RenderCurveEditor(FParticleEditorViewer* Viewer)
{
	DrawParticlePanelTitle("Curve Editor", "Channels");
	if (ImGui::SmallButton("Remove Curve"))
	{
		CurveSourceType = EParticleEditorSelectionType::None;
		CurveSourceEmitterIndex = -1;
		CurveSourceLODIndex = -1;
		CurveSourceModuleIndex = -1;
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("Remove All"))
	{
		CurveSourceType = EParticleEditorSelectionType::None;
		CurveSourceEmitterIndex = -1;
		CurveSourceLODIndex = -1;
		CurveSourceModuleIndex = -1;
	}
	ImGui::Separator();

	UParticleModule* Module = ResolveParticleModule(
		Viewer,
		CurveSourceType,
		CurveSourceEmitterIndex,
		CurveSourceLODIndex,
		CurveSourceModuleIndex);
	if (!Module)
	{
		ImGui::TextDisabled("Click a module curve icon in the emitter panel.");
		return;
	}

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
	const float HeaderHeight = 64.5f;
	const float HeaderPreviewSize = 52.0f;
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	const float SeparatorX = CardStart.x + CardWidth + EmitterSeparatorGap * 0.5f;
	const float SeparatorBottom = ImGui::GetWindowPos().y + ImGui::GetWindowHeight() - ImGui::GetStyle().WindowPadding.y;

	DrawList->AddLine(ImVec2(SeparatorX, CardStart.y), ImVec2(SeparatorX, SeparatorBottom), IM_COL32(58, 60, 68, 255), 1.0f);
	DrawList->AddRectFilled(CardStart, ImVec2(CardStart.x + CardWidth, CardStart.y + HeaderHeight), IM_COL32(33, 34, 38, 255), 0.0f);
	DrawList->AddRect(CardStart, ImVec2(CardStart.x + CardWidth, CardStart.y + HeaderHeight), IM_COL32(75, 75, 82, 255), 0.0f);

	auto DrawEmitterContextMenu = [&]()
	{
		bool bDeletedEmitter = false;
		bool bDeletedModule = false;
		if (ImGui::MenuItem("Delete Emitter", nullptr, false, HasDeletableSelectedEmitter(Viewer)))
		{
			Viewer->DeleteSelectedEmitter();
			bDeletedEmitter = true;
		}
		if (!bDeletedEmitter && ImGui::MenuItem("Delete Module", nullptr, false, HasDeletableSelectedModule(Viewer)))
		{
			Viewer->DeleteSelectedModule();
			bDeletedModule = true;
		}
		if (!bDeletedEmitter && !bDeletedModule && ImGui::MenuItem("Add Emitter"))
		{
			Viewer->AddEmitter();
		}
		if (!bDeletedEmitter && !bDeletedModule && LOD && ImGui::BeginMenu("Add Module"))
		{
			DrawParticleModuleClassMenu(Viewer);
			ImGui::EndMenu();
		}
	};

	ImGui::InvisibleButton("##EmitterHeader", ImVec2(CardWidth, 30.0f));
	if (ImGui::IsItemClicked())
	{
		Viewer->SelectEmitter(EmitterIndex);
	}
	if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
	{
		Viewer->SelectEmitter(EmitterIndex);
		Viewer->SelectLOD(LODIndex);
		ImGui::OpenPopup("EmitterHeaderContext");
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

	if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
	{
		FParticleEmitterDragPayload Payload = {
			EmitterIndex
		};
		ImGui::SetDragDropPayload(ParticleEmitterDragPayload, &Payload, sizeof(Payload));
		ImGui::Text("Emitter %d", EmitterIndex);
		ImGui::EndDragDropSource();
	}

	ImGui::SetCursorScreenPos(ImVec2(CardStart.x + 12.0f, CardStart.y + 11.0f));
	ImGui::Text("Emitter %d", EmitterIndex);
	ImGui::SetCursorScreenPos(ImVec2(CardStart.x + 12.0f, CardStart.y + 36.0f));
	bool bEnabled = LOD ? LOD->bEnabled : false;
	if (!LOD)
	{
		ImGui::BeginDisabled();
	}
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1.0f, 1.0f));
	const float EmitterHeaderControlSize = ImGui::GetFrameHeight();
	if (ImGui::Checkbox("##EmitterEnabled", &bEnabled) && LOD)
	{
		Viewer->CaptureUndoSnapshot("EditEmitterEnabled");
		LOD->bEnabled = bEnabled;
		Viewer->SelectEmitter(EmitterIndex);
		Viewer->SelectLOD(LODIndex);
		Viewer->MarkDirty();
		Viewer->RestartSimulation();
	}
	ImGui::PopStyleVar(2);
	if (!LOD)
	{
		ImGui::EndDisabled();
	}
	ImGui::SameLine(0.0f, 7.0f);
	bool bSolo = LOD ? LOD->bSolo : false;
	const ImVec2 SoloSize(EmitterHeaderControlSize, EmitterHeaderControlSize);
	if (!LOD)
	{
		ImGui::BeginDisabled();
	}
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
	ImGui::PushStyleColor(ImGuiCol_Button, bSolo ? ImVec4(0.22f, 0.39f, 0.54f, 1.0f) : ImVec4(0.16f, 0.17f, 0.20f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bSolo ? ImVec4(0.28f, 0.49f, 0.67f, 1.0f) : ImVec4(0.22f, 0.24f, 0.29f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, bSolo ? ImVec4(0.18f, 0.33f, 0.48f, 1.0f) : ImVec4(0.18f, 0.30f, 0.42f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_Text, bSolo ? ImVec4(0.70f, 0.90f, 1.0f, 1.0f) : ImVec4(0.62f, 0.65f, 0.70f, 1.0f));
	if (ImGui::Button("S##EmitterSolo", SoloSize) && LOD)
	{
		Viewer->CaptureUndoSnapshot("EditEmitterSolo");
		LOD->bSolo = !LOD->bSolo;
		Viewer->SelectEmitter(EmitterIndex);
		Viewer->SelectLOD(LODIndex);
		Viewer->MarkDirty();
		Viewer->RestartSimulation();
	}
	ImGui::PopStyleColor(4);
	ImGui::PopStyleVar(2);
	if (!LOD)
	{
		ImGui::EndDisabled();
	}
	ImGui::SetCursorScreenPos(ImVec2(CardStart.x + CardWidth - HeaderPreviewSize - 12.0f, CardStart.y + 6.0f));
	DrawEmitterPreview(ImVec2(HeaderPreviewSize, HeaderPreviewSize), EmitterIndex, bSelected);
	if (ImGui::IsItemClicked())
	{
		Viewer->SelectEmitter(EmitterIndex);
	}
	if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
	{
		Viewer->SelectEmitter(EmitterIndex);
		Viewer->SelectLOD(LODIndex);
		ImGui::OpenPopup("EmitterHeaderContext");
	}

	const ImVec2 HeaderEnd(CardStart.x + CardWidth, CardStart.y + HeaderHeight);
	const bool bHeaderHovered = ImGui::IsMouseHoveringRect(CardStart, HeaderEnd);
	const bool bHeaderControlHovered = ImGui::IsAnyItemHovered();
	if (bHeaderHovered && !bHeaderControlHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		Viewer->SelectEmitter(EmitterIndex);
	}
	if (bHeaderHovered && !bHeaderControlHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
	{
		Viewer->SelectEmitter(EmitterIndex);
		Viewer->SelectLOD(LODIndex);
		ImGui::OpenPopup("EmitterHeaderContext");
	}

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f)); // 창 가장자리 여백
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 6.0f));   // 메뉴 항목 간격
	if (ImGui::BeginPopup("EmitterHeaderContext"))
	{
		DrawEmitterContextMenu();
		ImGui::EndPopup();
	}
	ImGui::PopStyleVar(2);

	ImGui::SetCursorScreenPos(ImVec2(CardStart.x, CardStart.y + HeaderHeight + 6.0f));

	// 모듈 리스트 Child Window
	const float ModulesStartY = ImGui::GetCursorScreenPos().y;
	const float ChildHeight = SeparatorBottom - ModulesStartY;

	if (ChildHeight > 0.0f)
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 8.0f);

		ImGui::BeginChild("ModuleList", ImVec2(CardWidth, ChildHeight), false,
			ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysVerticalScrollbar);

		if (!LOD)
		{
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 8.0f);
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);
			ImGui::TextDisabled("No LOD");
		}
		else
		{
			if (LOD->RequiredModule)
			{
				DrawSelectableModuleRow(
					Viewer,
					GetObjectLabel(LOD->RequiredModule),
					EParticleEditorSelectionType::RequiredModule,
					EmitterIndex,
					LODIndex,
					-1,
					IM_COL32(244, 232, 156, 62),
					CurveSourceType,
					CurveSourceEmitterIndex,
					CurveSourceLODIndex,
					CurveSourceModuleIndex);
			}

			if (LOD->SpawnModule)
			{
				DrawSelectableModuleRow(
					Viewer,
					GetObjectLabel(LOD->SpawnModule),
					EParticleEditorSelectionType::SpawnModule,
					EmitterIndex,
					LODIndex,
					-1,
					IM_COL32(244, 150, 150, 58),
					CurveSourceType,
					CurveSourceEmitterIndex,
					CurveSourceLODIndex,
					CurveSourceModuleIndex);
			}

			for (int32 ModuleIndex = 0; ModuleIndex < static_cast<int32>(LOD->Modules.size()); ++ModuleIndex)
			{
				UParticleModule* Module = LOD->Modules[ModuleIndex];
				const bool bSpawnModule = Cast<UParticleModuleSpawn>(Module) != nullptr;
				DrawSelectableModuleRow(
					Viewer,
					GetObjectLabel(Module),
					EParticleEditorSelectionType::Module,
					EmitterIndex,
					LODIndex,
					ModuleIndex,
					bSpawnModule ? IM_COL32(244, 150, 150, 58) : IM_COL32(0, 0, 0, 0),
					CurveSourceType,
					CurveSourceEmitterIndex,
					CurveSourceLODIndex,
					CurveSourceModuleIndex);
			}

			if (LOD->TypeDataModule)
			{
				DrawSelectableModuleRow(
					Viewer,
					GetObjectLabel(LOD->TypeDataModule),
					EParticleEditorSelectionType::TypeDataModule,
					EmitterIndex,
					LODIndex,
					-1,
					IM_COL32(150, 190, 244, 45),
					CurveSourceType,
					CurveSourceEmitterIndex,
					CurveSourceLODIndex,
					CurveSourceModuleIndex);
			}
		}

		// Child Window 내부에 남은 빈 공간에 투명 버튼 생성
		const float EmptySpaceHeight = ImGui::GetContentRegionAvail().y;
		if (EmptySpaceHeight > 0.0f)
		{
			ImGui::InvisibleButton("##EmitterEmptySpace", ImVec2(ImGui::GetContentRegionAvail().x, EmptySpaceHeight));

			if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
			{
				Viewer->SelectEmitter(EmitterIndex);
			}
			else if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
			{
				Viewer->SelectEmitter(EmitterIndex);
				Viewer->SelectLOD(LODIndex);
			}

			// ==== [수정 부분] 빈 영역 컨텍스트 메뉴 여백 추가 ====
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 6.0f));
			if (ImGui::BeginPopupContextItem("EmitterEmptySpaceContext"))
			{
				DrawEmitterContextMenu();
				ImGui::EndPopup();
			}
			ImGui::PopStyleVar(2);

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
				ImGui::EndDragDropTarget();
			}
		}

		ImGui::EndChild();
		ImGui::PopStyleVar(2);
	}

	const ImVec2 CardEnd(CardStart.x + CardWidth, SeparatorBottom);
	if (bSelected)
	{
		const ImVec2 PanelMin = ImGui::GetWindowPos();
		const ImVec2 PanelMax(PanelMin.x + ImGui::GetWindowWidth(), PanelMin.y + ImGui::GetWindowHeight());
		ImDrawList* OutlineDrawList = IsAnyPopupOpen() ? DrawList : ImGui::GetForegroundDrawList();
		OutlineDrawList->PushClipRect(PanelMin, PanelMax, true);
		OutlineDrawList->AddRect(CardStart, CardEnd, IM_COL32(240, 219, 79, 255), 0.0f, 0, 2.0f);
		OutlineDrawList->PopClipRect();
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
		CurveSourceType = EParticleEditorSelectionType::Module;
		CurveSourceEmitterIndex = EmitterIndex;
		CurveSourceLODIndex = LODIndex;
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

HWND ResolveSaveDialogOwnerWindow(const UEditorEngine* EditorEngine)
{
	if (EditorEngine && EditorEngine->GetWindow())
	{
		return EditorEngine->GetWindow()->GetHWND();
	}

	if (const ImGuiViewport* MainViewport = ImGui::GetMainViewport())
	{
		if (MainViewport->PlatformHandleRaw)
		{
			return static_cast<HWND>(MainViewport->PlatformHandleRaw);
		}
	}

	return ::GetActiveWindow();
}

bool OpenParticleSaveFileDialog(HWND OwnerWindow, const FParticleEditorViewer* Viewer, FString& OutFilePath)
{
	OutFilePath.clear();

	constexpr DWORD ParticleDialogPathBufferLength = 32768;
	WCHAR FileBuffer[ParticleDialogPathBufferLength] = {};
	std::filesystem::path InitialDirectory = std::filesystem::path(FPaths::ToAbsolute(L"Asset/Particle")).lexically_normal();
	std::error_code ErrorCode;
	if (!std::filesystem::exists(InitialDirectory, ErrorCode) || !std::filesystem::is_directory(InitialDirectory, ErrorCode))
	{
		InitialDirectory = std::filesystem::path(FPaths::RootDir()).lexically_normal();
	}

	const FString CurrentFileName = Viewer ? FPaths::Normalize(Viewer->GetFileName()) : FString();
	if (!CurrentFileName.empty())
	{
		std::filesystem::path CurrentAbsolutePath = std::filesystem::path(FPaths::ToAbsolute(FPaths::ToWide(CurrentFileName))).lexically_normal();
		if (!CurrentAbsolutePath.parent_path().empty())
		{
			InitialDirectory = CurrentAbsolutePath.parent_path();
		}
		wcsncpy_s(FileBuffer, CurrentAbsolutePath.wstring().c_str(), _TRUNCATE);
	}

	OPENFILENAMEW DialogDesc = {};
	DialogDesc.lStructSize = sizeof(DialogDesc);
	DialogDesc.hwndOwner = OwnerWindow;
	DialogDesc.lpstrFilter = L"Particle System (*.particle)\0*.particle\0All Files (*.*)\0*.*\0";
	DialogDesc.lpstrFile = FileBuffer;
	DialogDesc.nMaxFile = ParticleDialogPathBufferLength;
	const std::wstring InitialDirectoryText = InitialDirectory.wstring();
	DialogDesc.lpstrInitialDir = InitialDirectoryText.c_str();
	DialogDesc.lpstrDefExt = L"particle";
	DialogDesc.Flags = OFN_EXPLORER | OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

	if (!GetSaveFileNameW(&DialogDesc))
	{
		const DWORD DialogError = CommDlgExtendedError();
		if (DialogError != 0)
		{
			WCHAR DebugMessage[128] = {};
			swprintf_s(DebugMessage, L"Particle Save As dialog failed. CommDlgExtendedError=0x%08X\n", DialogError);
			OutputDebugStringW(DebugMessage);
		}
		return false;
	}

	std::filesystem::path PickedPath(FileBuffer);
	if (PickedPath.extension().empty())
	{
		PickedPath.replace_extension(L".particle");
	}

	const std::filesystem::path RootPath(FPaths::RootDir());
	std::error_code RelativeErrorCode;
	std::filesystem::path RelativePath = std::filesystem::relative(PickedPath, RootPath, RelativeErrorCode);
	if (!RelativeErrorCode && !RelativePath.empty())
	{
		const std::wstring RelativeText = RelativePath.generic_wstring();
		if (RelativeText != L".." && RelativeText.rfind(L"../", 0) != 0)
		{
			OutFilePath = FPaths::Normalize(FPaths::ToUtf8(RelativeText));
			return true;
		}
	}

	OutFilePath = FPaths::Normalize(FPaths::ToUtf8(PickedPath.generic_wstring()));
	return !OutFilePath.empty();
}

const char* GetSelectionLabel(EParticleEditorSelectionType Type)
{
	switch (Type)
	{
	case EParticleEditorSelectionType::ParticleSystem:
		return "Particle System";
	case EParticleEditorSelectionType::Emitter:
		return "Emitter";
	case EParticleEditorSelectionType::LODLevel:
		return "LOD Level";
	case EParticleEditorSelectionType::RequiredModule:
	case EParticleEditorSelectionType::SpawnModule:
	case EParticleEditorSelectionType::TypeDataModule:
	case EParticleEditorSelectionType::Module:
		return "Module";
	case EParticleEditorSelectionType::None:
	default:
		return "None";
	}
}

const char* GetObjectLabel(const UObject* Object)
{
	if (Object && !UObjectManager::Get().ContainsObject(Object))
	{
		return "None";
	}

	const UClass* Class = Object ? Object->GetClass() : nullptr;
	return Class ? Class->GetDisplayName() : "None";
}

bool HasDeletableSelectedEmitter(FParticleEditorViewer* Viewer)
{
	UParticleSystem* ParticleSystem = Viewer ? Viewer->GetParticleSystem() : nullptr;
	const int32 SelectedEmitterIndex = Viewer ? Viewer->GetSelectedEmitterIndex() : -1;
	return ParticleSystem != nullptr &&
		SelectedEmitterIndex >= 0 &&
		SelectedEmitterIndex < static_cast<int32>(ParticleSystem->Emitters.size()) &&
		ParticleSystem->Emitters[SelectedEmitterIndex] != nullptr;
}

bool HasDeletableSelectedModule(FParticleEditorViewer* Viewer)
{
	if (!Viewer || Viewer->GetSelectionType() != EParticleEditorSelectionType::Module)
	{
		return false;
	}

	UParticleLODLevel* LOD = Viewer->GetSelectedLODLevel();
	const int32 SelectedModuleIndex = Viewer->GetSelectedModuleIndex();
	return LOD != nullptr &&
		SelectedModuleIndex >= 0 &&
		SelectedModuleIndex < static_cast<int32>(LOD->Modules.size()) &&
		LOD->Modules[SelectedModuleIndex] != nullptr;
}

bool IsAnyPopupOpen()
{
	ImGuiContext* Context = ImGui::GetCurrentContext();
	return Context && Context->OpenPopupStack.Size > 0;
}

void HandleParticleEditorShortcuts(FParticleEditorViewer* Viewer)
{
	if (!Viewer || IsAnyPopupOpen())
	{
		return;
	}

	const ImGuiIO& IO = ImGui::GetIO();
	if (IO.WantTextInput || ImGui::IsAnyItemActive())
	{
		return;
	}

	if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
	{
		return;
	}

	if (IO.KeyCtrl && !IO.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z, false))
	{
		Viewer->Undo();
		return;
	}

	if ((IO.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) ||
		(IO.KeyCtrl && IO.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z, false)))
	{
		Viewer->Redo();
		return;
	}

	if (ImGui::IsKeyPressed(ImGuiKey_Space, false))
	{
		Viewer->RestartSimulation();
		return;
	}

	if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
	{
		Viewer->DeleteSelection();
	}
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

const char* GetPropertyDisplayName(const FProperty& Property)
{
	return (Property.DisplayName && Property.DisplayName[0] != '\0') ? Property.DisplayName : Property.Name;
}

FString MakeParticlePropertyWidgetLabel(const FProperty& Property)
{
	const char* DisplayName = GetPropertyDisplayName(Property);
	if (!DisplayName)
	{
		return "";
	}
	if (!Property.Name || std::strcmp(DisplayName, Property.Name) == 0)
	{
		return DisplayName;
	}
	return FString(DisplayName) + "##" + Property.Name;
}

bool IsParticleGraphReferenceProperty(const FProperty& Property)
{
	if (Property.ReferenceKind != EObjectReferenceKind::RuntimeObject)
	{
		return false;
	}
	if (Property.Type == EPropertyType::ObjectPtr)
	{
		return true;
	}
	return Property.Type == EPropertyType::Array &&
		   Property.InnerProperty &&
		   Property.InnerProperty->Type == EPropertyType::ObjectPtr;
}

void CollectParticleEditableProperties(UObject* Object, TArray<const FProperty*>& OutProperties)
{
	if (!Object || !Object->GetClass())
	{
		return;
	}

	TArray<const FProperty*> AllProperties;
	Object->GetClass()->GetAllProperties(AllProperties);
	for (const FProperty* Property : AllProperties)
	{
		if (!Property || !Property->Name || !Property->IsEditable())
		{
			continue;
		}
		if (IsParticleGraphReferenceProperty(*Property))
		{
			continue;
		}
		OutProperties.push_back(Property);
	}
}

bool RenderParticleReflectionProperties(FParticleEditorViewer* Viewer, UObject* Object, UEditorEngine* EditorEngine, bool& bUndoCaptured)
{
	TArray<const FProperty*> Properties;
	CollectParticleEditableProperties(Object, Properties);
	for (const FProperty* Property : Properties)
	{
		if (Property)
		{
			RenderParticlePropertyWidget(Viewer, Object, *Property, EditorEngine, bUndoCaptured);
		}
	}
	return !Properties.empty();
}

bool RenderParticlePropertyWidget(FParticleEditorViewer* Viewer, UObject* Object, const FProperty& Property, UEditorEngine* EditorEngine, bool& bUndoCaptured)
{
	if (!Viewer || !Object || !Property.Name || !Property.IsEditable())
	{
		return false;
	}

	void* ValuePtr = Property.GetValuePtr(Object);
	if (!ValuePtr)
	{
		return false;
	}

	const FString Label = MakeParticlePropertyWidgetLabel(Property);
	const bool bChanged = RenderParticlePropertyValueWidget(Viewer, Object, Property, ValuePtr, Label.c_str(), EditorEngine, bUndoCaptured);
	if (ImGui::IsItemActivated() && !bUndoCaptured)
	{
		Viewer->CaptureUndoSnapshot("EditParticleProperty");
		bUndoCaptured = true;
	}

	if (bChanged)
	{
		if (!bUndoCaptured)
		{
			Viewer->CaptureUndoSnapshot("EditParticleProperty");
			bUndoCaptured = true;
		}
		Object->PostEditProperty(Property.Name);
		Viewer->MarkDirty();
		Viewer->RestartSimulation();
	}

	if (ImGui::IsItemDeactivatedAfterEdit() || !ImGui::IsAnyItemActive())
	{
		bUndoCaptured = false;
	}

	(void)EditorEngine;
	return bChanged;
}

bool RenderParticlePropertyValueWidget(FParticleEditorViewer* Viewer, UObject* Object, const FProperty& Property, void* ValuePtr, const char* Label, UEditorEngine* EditorEngine, bool& bUndoCaptured)
{
	if (!ValuePtr)
	{
		return false;
	}

	switch (Property.Type)
	{
	case EPropertyType::Bool:
		return ImGui::Checkbox(Label, static_cast<bool*>(ValuePtr));
	case EPropertyType::Int:
		return ImGui::DragInt(Label, static_cast<int32*>(ValuePtr), Property.Speed);
	case EPropertyType::Float:
		if (Property.Min != 0.0f || Property.Max != 0.0f)
		{
			return ImGui::DragFloat(Label, static_cast<float*>(ValuePtr), Property.Speed, Property.Min, Property.Max);
		}
		return ImGui::DragFloat(Label, static_cast<float*>(ValuePtr), Property.Speed);
	case EPropertyType::String:
	{
		FString* Value = static_cast<FString*>(ValuePtr);
		char Buffer[512];
		strncpy_s(Buffer, sizeof(Buffer), Value->c_str(), _TRUNCATE);
		if (ImGui::InputText(Label, Buffer, sizeof(Buffer)))
		{
			*Value = Buffer;
			return true;
		}
		return false;
	}
	case EPropertyType::Name:
	{
		FName* Value = static_cast<FName*>(ValuePtr);
		FString Current = Value->ToString();
		char Buffer[256];
		strncpy_s(Buffer, sizeof(Buffer), Current.c_str(), _TRUNCATE);
		if (ImGui::InputText(Label, Buffer, sizeof(Buffer)))
		{
			*Value = FName(Buffer);
			return true;
		}
		return false;
	}
	case EPropertyType::Enum:
	{
		if (!Property.EnumMeta || !Property.EnumMeta->Values || Property.EnumMeta->Count == 0)
		{
			return false;
		}

		int64 CurrentValue = 0;
		switch (Property.EnumMeta->Size)
		{
		case 1: CurrentValue = static_cast<int64>(*static_cast<uint8*>(ValuePtr)); break;
		case 2: CurrentValue = static_cast<int64>(*static_cast<uint16*>(ValuePtr)); break;
		case 4: CurrentValue = static_cast<int64>(*static_cast<int32*>(ValuePtr)); break;
		case 8: CurrentValue = static_cast<int64>(*static_cast<int64*>(ValuePtr)); break;
		default: break;
		}

		int32 CurrentIndex = 0;
		for (uint32 Index = 0; Index < Property.EnumMeta->Count; ++Index)
		{
			if (Property.EnumMeta->Values[Index].Value == CurrentValue)
			{
				CurrentIndex = static_cast<int32>(Index);
				break;
			}
		}

		const auto ComboGetter = [](void* Data, int Index) -> const char*
		{
			const UEnum* EnumMeta = static_cast<const UEnum*>(Data);
			if (!EnumMeta || Index < 0 || static_cast<uint32>(Index) >= EnumMeta->Count)
			{
				return "";
			}
			const FEnumValue& ValueMeta = EnumMeta->Values[Index];
			return (ValueMeta.DisplayName && ValueMeta.DisplayName[0] != '\0') ? ValueMeta.DisplayName : ValueMeta.Name;
		};

		if (ImGui::Combo(Label, &CurrentIndex, ComboGetter, const_cast<UEnum*>(Property.EnumMeta), static_cast<int>(Property.EnumMeta->Count)))
		{
			const int64 NewValue = Property.EnumMeta->Values[CurrentIndex].Value;
			switch (Property.EnumMeta->Size)
			{
			case 1: *static_cast<uint8*>(ValuePtr) = static_cast<uint8>(NewValue); break;
			case 2: *static_cast<uint16*>(ValuePtr) = static_cast<uint16>(NewValue); break;
			case 4: *static_cast<int32*>(ValuePtr) = static_cast<int32>(NewValue); break;
			case 8: *static_cast<int64*>(ValuePtr) = static_cast<int64>(NewValue); break;
			default: break;
			}
			return true;
		}
		return false;
	}
	case EPropertyType::Struct:
		return RenderParticleStructPropertyWidget(Viewer, Object, Property, ValuePtr, Label, EditorEngine, bUndoCaptured);
	case EPropertyType::ObjectPtr:
		return RenderParticleObjectPtrWidget(Property, ValuePtr, Label, EditorEngine);
	case EPropertyType::SoftObjectPtr:
		return RenderParticleSoftObjectPtrWidget(Property, ValuePtr, Label, EditorEngine);
	case EPropertyType::Array:
		return RenderParticleArrayPropertyWidget(Viewer, Object, Property, ValuePtr, EditorEngine, bUndoCaptured);
	default:
		break;
	}
	return false;
}

bool RenderParticleObjectPtrWidget(const FProperty& Property, void* ValuePtr, const char* Label, UEditorEngine* EditorEngine)
{
	if (!Property.ObjectPtrOps || !ValuePtr)
	{
		return false;
	}

	UObject* CurrentObject = Property.ObjectPtrOps->GetObject(ValuePtr);
	if (Property.ReferenceKind == EObjectReferenceKind::Asset &&
		Property.ObjectClass &&
		Property.ObjectClass->IsChildOf(UMaterialInterface::StaticClass()) &&
		EditorEngine)
	{
		const TArray<FString>& MaterialNames = EditorEngine->GetAssetService().GetMaterialInterfaceNames();
		UMaterialInterface* CurrentMaterial = Cast<UMaterialInterface>(CurrentObject);
		const FString CurrentLabel = CurrentMaterial
			? (CurrentMaterial->GetFilePath().empty() ? CurrentMaterial->GetName() : FPaths::Normalize(CurrentMaterial->GetFilePath()))
			: FString("<None>");

		bool bChanged = false;
		if (ImGui::BeginCombo(Label, CurrentLabel.c_str()))
		{
			if (ImGui::Selectable("<None>", CurrentMaterial == nullptr))
			{
				Property.ObjectPtrOps->SetObject(ValuePtr, nullptr);
				bChanged = true;
			}
			for (int32 MaterialIndex = 0; MaterialIndex < static_cast<int32>(MaterialNames.size()); ++MaterialIndex)
			{
				const FString& MaterialLabel = MaterialNames[MaterialIndex];
				const bool bSelected = CurrentLabel == MaterialLabel;
				if (ImGui::Selectable(MaterialLabel.c_str(), bSelected))
				{
					if (UMaterialInterface* Candidate = EditorEngine->GetAssetService().ResolveMaterialInterfaceByIndex(MaterialIndex))
					{
						Property.ObjectPtrOps->SetObject(ValuePtr, Candidate);
						bChanged = true;
					}
				}
				if (bSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		return bChanged;
	}

	const FString ObjectLabel = CurrentObject ? CurrentObject->GetClassName() : FString("None");
	ImGui::TextDisabled("%s: %s", Label, ObjectLabel.c_str());
	return false;
}

bool RenderParticleSoftObjectPtrWidget(const FProperty& Property, void* ValuePtr, const char* Label, UEditorEngine* EditorEngine)
{
	if (!Property.SoftObjectOps || !ValuePtr)
	{
		return false;
	}

	FString Current = Property.SoftObjectOps->GetPath(ValuePtr);
	TArray<FString> LocalOptions;
	const TArray<FString>* Options = nullptr;
	if (Property.ObjectClass)
	{
		if (Property.ObjectClass->IsChildOf(UStaticMesh::StaticClass()) && EditorEngine)
		{
			Options = &EditorEngine->GetAssetService().GetStaticMeshAssetPaths();
		}
		else if (Property.ObjectClass->IsChildOf(UCurveFloatAsset::StaticClass()))
		{
			LocalOptions = FResourceManager::Get().GetCurvePaths();
			Options = &LocalOptions;
		}
		else if (Property.ObjectClass->IsChildOf(UParticleSystem::StaticClass()))
		{
			LocalOptions = FResourceManager::Get().GetParticleSystemPaths();
			Options = &LocalOptions;
		}
	}

	bool bChanged = false;
	if (Options && !Options->empty())
	{
		if (ImGui::BeginCombo(Label, Current.empty() ? "<None>" : Current.c_str()))
		{
			if (ImGui::Selectable("<None>", Current.empty()))
			{
				Property.SoftObjectOps->SetPath(ValuePtr, FString());
				bChanged = true;
			}
			for (const FString& Path : *Options)
			{
				const bool bSelected = Current == Path;
				if (ImGui::Selectable(Path.c_str(), bSelected))
				{
					Property.SoftObjectOps->SetPath(ValuePtr, Path);
					bChanged = true;
				}
				if (bSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
	}
	else
	{
		char Buffer[512];
		strncpy_s(Buffer, sizeof(Buffer), Current.c_str(), _TRUNCATE);
		if (ImGui::InputText(Label, Buffer, sizeof(Buffer)))
		{
			Property.SoftObjectOps->SetPath(ValuePtr, Buffer);
			bChanged = true;
		}
	}
	return bChanged;
}

bool RenderParticleArrayPropertyWidget(FParticleEditorViewer* Viewer, UObject* Object, const FProperty& Property, void* ValuePtr, UEditorEngine* EditorEngine, bool& bUndoCaptured)
{
	if (!Property.ArrayOps || !Property.InnerProperty || !ValuePtr)
	{
		return false;
	}
	if (IsParticleGraphReferenceProperty(Property))
	{
		ImGui::TextDisabled("%s is managed by the emitter graph.", GetPropertyDisplayName(Property));
		return false;
	}

	bool bChanged = false;
	int32 RemoveIndex = -1;
	DrawParticleDetailsSection(GetPropertyDisplayName(Property));
	ImGui::PushID(Property.Name);

	const int32 Count = Property.ArrayOps->Num(ValuePtr);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		ImGui::PushID(Index);
		void* ElementPtr = Property.ArrayOps->GetElementPtr(ValuePtr, Index);
		char ItemLabel[32];
		snprintf(ItemLabel, sizeof(ItemLabel), "[%d]", Index);
		ImGui::SetNextItemWidth(std::max(120.0f, ImGui::GetContentRegionAvail().x - 28.0f));
		if (RenderParticlePropertyValueWidget(Viewer, Object, *Property.InnerProperty, ElementPtr, ItemLabel, EditorEngine, bUndoCaptured))
		{
			bChanged = true;
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("X"))
		{
			RemoveIndex = Index;
		}
		ImGui::PopID();
	}

	if (RemoveIndex >= 0)
	{
		Property.ArrayOps->RemoveAt(ValuePtr, RemoveIndex);
		bChanged = true;
	}

	char AddLabel[64];
	snprintf(AddLabel, sizeof(AddLabel), "+ Add##%s", Property.Name);
	if (ImGui::Button(AddLabel, ImVec2(-1, 0.0f)))
	{
		Property.ArrayOps->AddDefaulted(ValuePtr);
		bChanged = true;
	}

	ImGui::PopID();
	return bChanged;
}

bool RenderParticleStructPropertyWidget(FParticleEditorViewer* Viewer, UObject* Object, const FProperty& Property, void* ValuePtr, const char* Label, UEditorEngine* EditorEngine, bool& bUndoCaptured)
{
	if (!ValuePtr || Property.Type != EPropertyType::Struct)
	{
		return false;
	}

	const char* Hint = Property.EditorHint;
	if ((!Hint || Hint[0] == '\0') && Property.ScriptStruct)
	{
		Hint = Property.ScriptStruct->GetName();
	}

	if (Hint && std::strcmp(Hint, "FVector") == 0)
	{
		return ImGui::DragFloat3(Label, static_cast<float*>(ValuePtr), Property.Speed);
	}
	if (Hint && std::strcmp(Hint, "FVector4") == 0)
	{
		return ImGui::DragFloat4(Label, static_cast<float*>(ValuePtr), Property.Speed);
	}
	if (Hint && std::strcmp(Hint, "FColor") == 0)
	{
		return ImGui::ColorEdit4(Label, &static_cast<FColor*>(ValuePtr)->R);
	}
	if (Hint && std::strcmp(Hint, "FQuat") == 0)
	{
		FQuat* Value = static_cast<FQuat*>(ValuePtr);
		float Components[4] = { Value->X, Value->Y, Value->Z, Value->W };
		if (ImGui::DragFloat4(Label, Components, Property.Speed))
		{
			*Value = FQuat(Components[0], Components[1], Components[2], Components[3]);
			Value->Normalize();
			return true;
		}
		return false;
	}

	if (!Property.ScriptStruct)
	{
		ImGui::TextDisabled("%s <unregistered struct>", Label);
		return false;
	}

	bool bChanged = false;
	if (ImGui::TreeNodeEx(Label, ImGuiTreeNodeFlags_DefaultOpen))
	{
		TArray<const FProperty*> ChildProperties;
		Property.ScriptStruct->GetAllProperties(ChildProperties);
		for (const FProperty* Child : ChildProperties)
		{
			if (!Child || !Child->Name || !Child->IsEditable())
			{
				continue;
			}
			void* ChildPtr = reinterpret_cast<uint8*>(ValuePtr) + Child->Offset;
			const FString ChildLabel = MakeParticlePropertyWidgetLabel(*Child);
			if (RenderParticlePropertyValueWidget(Viewer, Object, *Child, ChildPtr, ChildLabel.c_str(), EditorEngine, bUndoCaptured))
			{
				bChanged = true;
			}
		}
		ImGui::TreePop();
	}
	return bChanged;
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
					   !Class->HasAnyClassFlags(CF_Placeable) ||
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
	const ImVec2 Min = ImGui::GetItemRectMin();
	const ImVec2 Max = ImGui::GetItemRectMax();
	const ImVec2 Center((Min.x + Max.x) * 0.5f, (Min.y + Max.y) * 0.5f);
	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	const ImU32 FillColor = bHovered ? IM_COL32(58, 64, 76, 178) : IM_COL32(38, 42, 50, 138);
	const ImU32 BorderColor = bHovered ? IM_COL32(122, 136, 160, 210) : IM_COL32(88, 96, 112, 165);
	DrawList->AddRectFilled(Min, Max, FillColor, 6.0f);
	DrawList->AddRect(Min, Max, BorderColor, 6.0f);

	const ImVec2 TextSize = ImGui::CalcTextSize(Label);
	DrawList->AddText(
		ImVec2(Center.x - TextSize.x * 0.5f, Center.y - TextSize.y * 0.5f),
		ImGui::GetColorU32(bHovered ? ImGuiCol_Text : ImGuiCol_TextDisabled),
		Label);

	if (bHovered && Tooltip && Tooltip[0] != '\0')
	{
		ImGui::SetTooltip("%s", Tooltip);
	}
	ImGui::PopID();
	return bPressed;
}

bool DrawCascadeGraphButton(const char* Id, const ImVec2& Size, bool bActive)
{
	ImGui::PushID(Id);
	const bool bPressed = ImGui::InvisibleButton("##CascadeGraphButton", Size);
	const bool bHovered = ImGui::IsItemHovered();
	const ImVec2 Min = ImGui::GetItemRectMin();
	const ImVec2 Max = ImGui::GetItemRectMax();
	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	const ImU32 BorderColor = bActive
		? IM_COL32(190, 236, 120, 255)
		: (bHovered ? IM_COL32(156, 198, 95, 255) : IM_COL32(86, 132, 62, 255));
	const ImU32 FillColor = bActive
		? IM_COL32(34, 64, 30, 255)
		: IM_COL32(18, 30, 20, 255);
	DrawList->AddRectFilled(Min, Max, FillColor, 0.0f);
	DrawList->AddRect(Min, Max, BorderColor, 0.0f);

	const float Pad = 3.0f;
	DrawList->AddLine(
		ImVec2(Min.x + Pad, Max.y - Pad),
		ImVec2(Min.x + Size.x * 0.42f, Min.y + Size.y * 0.55f),
		BorderColor,
		1.0f);
	DrawList->AddLine(
		ImVec2(Min.x + Size.x * 0.42f, Min.y + Size.y * 0.55f),
		ImVec2(Max.x - Pad, Min.y + Pad),
		BorderColor,
		1.0f);

	ImGui::PopID();
	return bPressed;
}

bool DrawCurrentLODToolbarInput(FParticleEditorViewer* Viewer, ID3D11ShaderResourceView* Icon, const ImVec2& IconSize, const ImVec2& Size)
{
	ImGui::PushID("CurrentLODToolbarInput");

	static FParticleEditorViewer* BufferedViewer = nullptr;
	static int32 BufferedLOD = -1;
	static bool bEditing = false;
	static char LODBuffer[16] = {};

	const int32 CurrentLODIndex = std::max(0, Viewer ? Viewer->GetSelectedLODIndex() : 0);
	if (!bEditing || BufferedViewer != Viewer || BufferedLOD != CurrentLODIndex)
	{
		BufferedViewer = Viewer;
		BufferedLOD = CurrentLODIndex;
		snprintf(LODBuffer, sizeof(LODBuffer), "%d", CurrentLODIndex);
	}

	const ImVec2 Start = ImGui::GetCursorScreenPos();
	const ImVec2 End(Start.x + Size.x, Start.y + Size.y);
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	const bool bHovered = ImGui::IsMouseHoveringRect(Start, End);
	const ImU32 BgColor = ImGui::GetColorU32(bHovered ? ImVec4(0.14f, 0.16f, 0.19f, 1.0f) : ImVec4(0.09f, 0.10f, 0.12f, 1.0f));
	DrawList->AddRectFilled(Start, End, BgColor, 3.0f);

	if (Icon)
	{
		const float Padding = std::max(3.0f, IconSize.x * 0.16f);
		DrawList->AddImage(
			reinterpret_cast<ImTextureID>(Icon),
			ImVec2(Start.x + Padding, Start.y + Padding),
			ImVec2(Start.x + IconSize.x - Padding, Start.y + IconSize.y - Padding));
	}

	const char* Prefix = "LOD:";
	const ImVec2 PrefixSize = ImGui::CalcTextSize(Prefix);
	const float PrefixX = Start.x + IconSize.x + 6.0f;
	DrawList->AddText(
		ImVec2(PrefixX, Start.y + (Size.y - PrefixSize.y) * 0.5f),
		ImGui::GetColorU32(ImVec4(0.94f, 0.95f, 0.98f, 1.0f)),
		Prefix);

	const float InputWidth = 36.0f;
	const float InputHeight = std::max(18.0f, Size.y - 6.0f);
	const ImVec2 InputPos(PrefixX + PrefixSize.x + 4.0f, Start.y + (Size.y - InputHeight) * 0.5f);
	ImGui::SetCursorScreenPos(InputPos);
	ImGui::SetNextItemWidth(InputWidth);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(3.0f, 1.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
	ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.04f, 0.05f, 0.07f, 0.88f));
	ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.08f, 0.10f, 0.13f, 0.95f));
	ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.10f, 0.12f, 0.15f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.98f, 0.98f, 1.0f, 1.0f));
	const bool bEnterPressed = ImGui::InputText(
		"##CurrentLODValue",
		LODBuffer,
		sizeof(LODBuffer),
		ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_CallbackCharFilter,
		FilterUnsignedIntegerInput);
	const bool bActivated = ImGui::IsItemActivated();
	const bool bActive = ImGui::IsItemActive();
	const bool bDeactivatedAfterEdit = ImGui::IsItemDeactivatedAfterEdit();
	ImGui::PopStyleColor(4);
	ImGui::PopStyleVar(2);

	if (bActivated)
	{
		bEditing = true;
	}

	bool bCommitted = false;
	if ((bEnterPressed || bDeactivatedAfterEdit) && Viewer)
	{
		const int32 LODCount = GetSelectedEmitterLODCount(Viewer);
		if (LODCount > 0 && IsUnsignedIntegerText(LODBuffer))
		{
			const int32 RequestedLOD = static_cast<int32>(std::strtol(LODBuffer, nullptr, 10));
			const int32 ClampedLOD = std::clamp(RequestedLOD, 0, LODCount - 1);
			Viewer->SelectLOD(ClampedLOD);
			snprintf(LODBuffer, sizeof(LODBuffer), "%d", ClampedLOD);
			BufferedLOD = ClampedLOD;
			bCommitted = true;
		}
		else
		{
			snprintf(LODBuffer, sizeof(LODBuffer), "%d", CurrentLODIndex);
			BufferedLOD = CurrentLODIndex;
		}
		bEditing = false;
	}
	else if (!bActive && bEditing)
	{
		snprintf(LODBuffer, sizeof(LODBuffer), "%d", CurrentLODIndex);
		BufferedLOD = CurrentLODIndex;
		bEditing = false;
	}

	if (bHovered)
	{
		ImGui::SetTooltip("Current LOD");
	}

	ImGui::SetCursorScreenPos(ImVec2(Start.x + Size.x, Start.y));
	ImGui::Dummy(ImVec2(0.0f, Size.y));
	ImGui::PopID();
	return bCommitted;
}

UParticleLODLevel* ResolveParticleLOD(FParticleEditorViewer* Viewer, int32 EmitterIndex, int32 LODIndex)
{
	UParticleSystem* ParticleSystem = Viewer ? Viewer->GetParticleSystem() : nullptr;
	if (!ParticleSystem || EmitterIndex < 0 || EmitterIndex >= static_cast<int32>(ParticleSystem->Emitters.size()))
	{
		return nullptr;
	}

	UParticleEmitter* Emitter = ParticleSystem->Emitters[EmitterIndex];
	if (!Emitter || LODIndex < 0 || LODIndex >= static_cast<int32>(Emitter->LODLevels.size()))
	{
		return nullptr;
	}

	return Emitter->LODLevels[LODIndex];
}

UParticleModule* ResolveParticleModule(
	FParticleEditorViewer* Viewer,
	EParticleEditorSelectionType Type,
	int32 EmitterIndex,
	int32 LODIndex,
	int32 ModuleIndex)
{
	UParticleLODLevel* LOD = ResolveParticleLOD(Viewer, EmitterIndex, LODIndex);
	if (!LOD)
	{
		return nullptr;
	}

	switch (Type)
	{
	case EParticleEditorSelectionType::RequiredModule:
		return LOD->RequiredModule;
	case EParticleEditorSelectionType::SpawnModule:
		return LOD->SpawnModule;
	case EParticleEditorSelectionType::TypeDataModule:
		return LOD->TypeDataModule;
	case EParticleEditorSelectionType::Module:
		return ModuleIndex >= 0 && ModuleIndex < static_cast<int32>(LOD->Modules.size())
			? LOD->Modules[ModuleIndex]
			: nullptr;
	default:
		return nullptr;
	}
}

void SelectParticleModuleTarget(
	FParticleEditorViewer* Viewer,
	EParticleEditorSelectionType Type,
	int32 EmitterIndex,
	int32 LODIndex,
	int32 ModuleIndex)
{
	if (!Viewer)
	{
		return;
	}

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
	default:
		break;
	}
}

int32 GetSelectedEmitterLODCount(FParticleEditorViewer* Viewer)
{
	UParticleEmitter* Emitter = Viewer ? Viewer->GetSelectedEmitter() : nullptr;
	return Emitter ? static_cast<int32>(Emitter->LODLevels.size()) : 0;
}

bool IsUnsignedIntegerText(const char* Text)
{
	if (!Text || Text[0] == '\0')
	{
		return false;
	}

	for (const char* It = Text; *It != '\0'; ++It)
	{
		if (*It < '0' || *It > '9')
		{
			return false;
		}
	}

	return true;
}

int FilterUnsignedIntegerInput(ImGuiInputTextCallbackData* Data)
{
	if (!Data)
	{
		return 1;
	}

	return Data->EventChar >= '0' && Data->EventChar <= '9' ? 0 : 1;
}

void DrawEmitterPreview(const ImVec2& Size, int32 EmitterIndex, bool bSelected)
{
	const ImVec2 Start = ImGui::GetCursorScreenPos();
	ImGui::InvisibleButton("##EmitterPreview", Size);

	const ImVec2 End(Start.x + Size.x, Start.y + Size.y);
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->AddRectFilled(Start, End, IM_COL32(18, 20, 24, 255), 0.0f);
	DrawList->AddRect(Start, End, bSelected ? IM_COL32(240, 219, 79, 255) : IM_COL32(76, 78, 86, 255), 0.0f);

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
	ImU32 BackgroundColor,
	EParticleEditorSelectionType& CurveSourceType,
	int32& CurveSourceEmitterIndex,
	int32& CurveSourceLODIndex,
	int32& CurveSourceModuleIndex)
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
	const float ControlSize = 16.0f;
	const float ControlGap = 5.0f;
	const float RowWidth = EmitterNodeWidth;
	const float SelectableWidth = std::max(1.0f, RowWidth - (ControlSize * 2.0f + ControlGap + 8.0f));
	const ImVec2 RowEnd(RowStart.x + RowWidth, RowStart.y + RowHeight);
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	if ((BackgroundColor & IM_COL32_A_MASK) != 0)
	{
		DrawList->AddRectFilled(
			RowStart,
			RowEnd,
			BackgroundColor,
			0.0f);
	}

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
	const bool bPressed = ImGui::InvisibleButton("##SelectableModuleRow", ImVec2(SelectableWidth, RowHeight));
	ImGui::PopStyleVar();
	const bool bRowHovered = ImGui::IsItemHovered();

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
	}
	HandleModuleDragDropTarget(Viewer, Type, EmitterIndex, LODIndex, ModuleIndex);

	if (bSelected || bRowHovered)
	{
		const ImU32 StateColor = ImGui::GetColorU32(
			bSelected
				? ImVec4(0.22f, 0.33f, 0.55f, 0.78f)
				: ImVec4(0.22f, 0.24f, 0.30f, 0.42f));
		DrawList->AddRectFilled(RowStart, RowEnd, StateColor, 0.0f);
	}

	const ImVec2 TextSize = ImGui::CalcTextSize(Label);
	DrawList->AddText(
		ImVec2(RowStart.x + TextLeftPadding, RowStart.y + (RowHeight - TextSize.y) * 0.5f),
		ImGui::GetColorU32(ImGuiCol_Text),
		Label);

	UParticleModule* Module = ResolveParticleModule(Viewer, Type, EmitterIndex, LODIndex, ModuleIndex);
	if (Module)
	{
		const float RightControlInset = ImGui::GetStyle().ScrollbarSize + 4.0f;
		const float GraphX = RowEnd.x - RightControlInset - ControlSize;
		const float CheckX = GraphX - ControlGap - ControlSize;
		const float ControlY = RowStart.y + (RowHeight - ControlSize) * 0.5f;

		ImGui::SetCursorScreenPos(ImVec2(CheckX, ControlY));
		bool bModuleEnabled = Module->bEnabled;
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1.0f, 1.0f));
		if (ImGui::Checkbox("##ModuleEnabled", &bModuleEnabled))
		{
			Viewer->CaptureUndoSnapshot("EditModuleEnabled");
			Module->bEnabled = bModuleEnabled;
			SelectParticleModuleTarget(Viewer, Type, EmitterIndex, LODIndex, ModuleIndex);
			Viewer->MarkDirty();
			Viewer->RestartSimulation();
		}
		ImGui::PopStyleVar(2);

		const bool bCurveActive =
			CurveSourceType == Type &&
			CurveSourceEmitterIndex == EmitterIndex &&
			CurveSourceLODIndex == LODIndex &&
			CurveSourceModuleIndex == ModuleIndex;
		ImGui::SetCursorScreenPos(ImVec2(GraphX, ControlY));
		if (DrawCascadeGraphButton("##ModuleCurve", ImVec2(ControlSize, ControlSize), bCurveActive))
		{
			CurveSourceType = Type;
			CurveSourceEmitterIndex = EmitterIndex;
			CurveSourceLODIndex = LODIndex;
			CurveSourceModuleIndex = ModuleIndex;
			SelectParticleModuleTarget(Viewer, Type, EmitterIndex, LODIndex, ModuleIndex);
		}
	}

	if (bPressed)
	{
		SelectParticleModuleTarget(Viewer, Type, EmitterIndex, LODIndex, ModuleIndex);
	}

	if (Type == EParticleEditorSelectionType::Module)
	{
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

	ImGui::SetCursorScreenPos(ImVec2(RowStart.x, RowEnd.y));
	ImGui::PopID();
	ImGui::PopID();
}

void HandleModuleDragDropTarget(FParticleEditorViewer* Viewer, EParticleEditorSelectionType Type, int32 EmitterIndex, int32 LODIndex, int32 ModuleIndex)
{
	if (!ImGui::BeginDragDropTarget())
	{
		return;
	}

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
} // namespace
