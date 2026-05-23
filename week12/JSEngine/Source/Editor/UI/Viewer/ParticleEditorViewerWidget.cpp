#include "ParticleEditorViewerWidget.h"

#include "Core/Reflection/ReflectionRegistry.h"
#include "Editor/UI/EditorMainPanelViewportToolbarHelpers.h"
#include "Editor/Viewer/EditorViewer.h"
#include "Editor/Viewer/ParticleEditorViewer.h"
#include "Editor/Viewport/FSceneViewport.h"
#include "Object/Class.h"
#include "Particle/ParticleAsset.h"
#include "imgui.h"
#include "imgui_internal.h"

#include <algorithm>
#include <cstring>

namespace
{
	constexpr const char* ParticleModuleDragPayload = "ParticleModule";

	struct FParticleModuleDragPayload
	{
		int32 EmitterIndex = -1;
		int32 LODIndex = -1;
		int32 ModuleIndex = -1;
	};

	FParticleEditorViewer* AsParticleViewer(FEditorViewer* Viewer);
	const char* GetSelectionLabel(EParticleEditorSelectionType Type);
	const char* GetObjectLabel(const UObject* Object);
	void GetParticleModuleClasses(TArray<UClass*>& OutClasses);
	bool DrawParticleModuleClassMenu(FParticleEditorViewer* Viewer);
	void DrawViewModeMenuItems(FParticleEditorViewer* Viewer);
	bool DrawPopupButton(const char* Label, const char* PopupId);
	void DrawEmitterPreview(const ImVec2& Size, int32 EmitterIndex, bool bSelected);
	void DrawSelectableModuleRow(
		FParticleEditorViewer* Viewer,
		const char* Label,
		EParticleEditorSelectionType Type,
		int32 EmitterIndex,
		int32 LODIndex,
		int32 ModuleIndex,
		ImU32 BackgroundColor);
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

	RenderMenuBar(ParticleViewer);
	RenderToolbar(ParticleViewer);

	const ImVec2 DockSize = ImGui::GetContentRegionAvail();
	if (DockSize.x <= 0.0f || DockSize.y <= 0.0f)
	{
		return;
	}

	ImGui::PushID(ParticleViewer);
	ImGuiID DockspaceId = ImGui::GetID("ParticleEditorDockspace");
	ImGui::DockSpace(DockspaceId, DockSize, ImGuiDockNodeFlags_None);

	const char* ViewportWindowName = "Viewport##ParticleViewportDock";
	const char* EmitterWindowName = "Emitters##ParticleEmitterDock";
	const char* DetailsWindowName = "Details##ParticleDetailsDock";
	const char* CurveWindowName = "Curve Editor##ParticleCurveDock";

	if (!bDockLayoutInitialized)
	{
		bDockLayoutInitialized = true;
		ImGui::DockBuilderRemoveNode(DockspaceId);
		ImGui::DockBuilderAddNode(DockspaceId, ImGuiDockNodeFlags_DockSpace);
		ImGui::DockBuilderSetNodeSize(DockspaceId, DockSize);

		ImGuiID DockLeft = DockspaceId;
		ImGuiID DockRight = 0;
		ImGuiID DockViewport = 0;
		ImGuiID DockDetails = 0;
		ImGuiID DockEmitter = 0;
		ImGuiID DockCurve = 0;
		const float EmitterSplitRatio = std::clamp(EmitterPanelWidthRatio, 0.2f, 0.85f);
		const float BottomSplitRatio = std::clamp(BottomPanelHeightRatio, 0.2f, 0.8f);
		ImGui::DockBuilderSplitNode(DockLeft, ImGuiDir_Right, EmitterSplitRatio, &DockRight, &DockLeft);
		ImGui::DockBuilderSplitNode(DockLeft, ImGuiDir_Down, BottomSplitRatio, &DockDetails, &DockViewport);
		ImGui::DockBuilderSplitNode(DockRight, ImGuiDir_Down, BottomSplitRatio, &DockCurve, &DockEmitter);
		ImGui::DockBuilderDockWindow(ViewportWindowName, DockViewport);
		ImGui::DockBuilderDockWindow(DetailsWindowName, DockDetails);
		ImGui::DockBuilderDockWindow(EmitterWindowName, DockEmitter);
		ImGui::DockBuilderDockWindow(CurveWindowName, DockCurve);
		ImGui::DockBuilderFinish(DockspaceId);
	}

	ImGuiWindowFlags PanelFlags = ImGuiWindowFlags_NoCollapse;
	if (ImGui::Begin(ViewportWindowName, nullptr, PanelFlags | ImGuiWindowFlags_MenuBar))
	{
		RenderViewportOptions(ParticleViewer);
		RenderViewportPanel(SceneViewport, SRV, ImGui::GetContentRegionAvail());
	}
	ImGui::End();
	if (ImGui::Begin(EmitterWindowName, nullptr, PanelFlags))
	{
		RenderEmitterPanel(ParticleViewer);
	}
	ImGui::End();
	if (ImGui::Begin(DetailsWindowName, nullptr, PanelFlags))
	{
		RenderDetailsPanel(ParticleViewer);
	}
	ImGui::End();
	if (ImGui::Begin(CurveWindowName, nullptr, PanelFlags))
	{
		RenderCurveEditor(ParticleViewer);
	}
	ImGui::End();

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
				bDockLayoutInitialized = false;
			}
			float BottomRatio = BottomPanelHeightRatio;
			if (ImGui::SliderFloat("Bottom Height", &BottomRatio, 0.2f, 0.8f, "%.2f"))
			{
				BottomPanelHeightRatio = BottomRatio;
				bDockLayoutInitialized = false;
			}
			if (ImGui::MenuItem("Reset Particle Layout"))
			{
				EmitterPanelWidthRatio = 2.0f / 3.0f;
				BottomPanelHeightRatio = 0.5f;
				bDockLayoutInitialized = false;
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

	ImGui::SameLine();
	if (DrawPopupButton("View", "##ParticleTopViewMenu"))
	{
		if (ImGui::BeginPopup("##ParticleTopViewMenu"))
		{
			if (ImGui::BeginMenu("View Modes"))
			{
				DrawViewModeMenuItems(Viewer);
				ImGui::EndMenu();
			}
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
			ImGui::EndPopup();
		}
	}
	ImGui::SameLine();
	if (DrawPopupButton("Time", "##ParticleTopTimeMenu"))
	{
		if (ImGui::BeginPopup("##ParticleTopTimeMenu"))
		{
			RenderTimeControls(Viewer);
			ImGui::EndPopup();
		}
	}

	ImGui::PopStyleVar();
	ImGui::EndChild();
}

void FParticleEditorViewerWidget::RenderToolbar(FParticleEditorViewer* Viewer)
{
	ImGui::BeginChild("ParticleToolbar", ImVec2(0.0f, 34.0f), false);

	if (ImGui::Button("Save"))
	{
		Viewer->Save();
	}
	ImGui::SameLine();
	if (ImGui::Button("Find"))
	{
		Viewer->FindInContentBrowser();
	}
	ImGui::SameLine();
	if (ImGui::Button("Restart Sim"))
	{
		Viewer->RestartSimulation();
	}
	ImGui::SameLine();
	if (ImGui::Button("Restart Level"))
	{
		Viewer->RestartLevel();
	}
	ImGui::SameLine();
	ImGui::BeginDisabled();
	ImGui::Button("Undo");
	ImGui::SameLine();
	ImGui::Button("Redo");
	ImGui::EndDisabled();
	ImGui::SameLine();
	bool bBounds = Viewer->IsShowBounds();
	if (ImGui::Checkbox("Bounds", &bBounds))
	{
		Viewer->SetShowBounds(bBounds);
	}
	ImGui::SameLine();
	FColor Background = Viewer->GetBackgroundColor();
	float Color[4] = { Background.R, Background.G, Background.B, Background.A };
	if (ImGui::ColorEdit4("Background", Color, ImGuiColorEditFlags_NoInputs))
	{
		Viewer->SetBackgroundColor(FColor(Color[0], Color[1], Color[2], Color[3]));
	}
	ImGui::SameLine();
	if (ImGui::Button("Highest LOD"))
	{
		Viewer->SetHighestLOD();
	}
	ImGui::SameLine();
	if (ImGui::Button("Lower LOD"))
	{
		Viewer->SelectLowerLOD();
	}
	ImGui::SameLine();
	if (ImGui::Button("Upper LOD"))
	{
		Viewer->SelectUpperLOD();
	}

	ImGui::EndChild();
}

void FParticleEditorViewerWidget::RenderViewportOptions(FParticleEditorViewer* Viewer)
{
	if (!ImGui::BeginMenuBar())
	{
		return;
	}

	if (ImGui::BeginMenu("View"))
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
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Time"))
	{
		RenderTimeControls(Viewer);
		ImGui::EndMenu();
	}
	ImGui::EndMenuBar();
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

void FParticleEditorViewerWidget::RenderEmitterPanel(FParticleEditorViewer* Viewer)
{
	UParticleSystem* ParticleSystem = Viewer->GetParticleSystem();
	ImGui::TextUnformatted("Emitters");
	ImGui::SameLine();
	if (ImGui::SmallButton("+ Emitter"))
	{
		Viewer->AddEmitter();
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("- Emitter"))
	{
		Viewer->DeleteSelectedEmitter();
	}
	ImGui::Separator();

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

	if (ImGui::BeginTable("##ParticleEmitterColumns", std::max(1, static_cast<int32>(ParticleSystem->Emitters.size())), ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable))
	{
		for (int32 EmitterIndex = 0; EmitterIndex < static_cast<int32>(ParticleSystem->Emitters.size()); ++EmitterIndex)
		{
			ImGui::TableSetupColumn(("Emitter " + std::to_string(EmitterIndex)).c_str(), ImGuiTableColumnFlags_WidthFixed, 220.0f);
		}
		ImGui::TableNextRow();
		for (int32 EmitterIndex = 0; EmitterIndex < static_cast<int32>(ParticleSystem->Emitters.size()); ++EmitterIndex)
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
		if (ImGui::BeginMenu("Add Module", Viewer->GetSelectedLODLevel() != nullptr))
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
	ImGui::Text("Details: %s", GetSelectionLabel(Viewer->GetSelectionType()));
	ImGui::TextDisabled("%s", GetObjectLabel(SelectedObject));
	ImGui::Separator();

	if (!SelectedObject)
	{
		ImGui::TextDisabled("Select a particle system, emitter, LOD, or module.");
		return;
	}

	if (Viewer->GetSelectionType() == EParticleEditorSelectionType::ParticleSystem)
	{
		UParticleSystem* ParticleSystem = Viewer->GetParticleSystem();
		ImGui::Text("Emitter Count: %d", ParticleSystem ? static_cast<int32>(ParticleSystem->Emitters.size()) : 0);
		return;
	}

	if (UParticleLODLevel* LODLevel = Cast<UParticleLODLevel>(SelectedObject))
	{
		if (ImGui::InputInt("Level", &LODLevel->Level))
		{
			Viewer->MarkDirty();
		}
		if (ImGui::Checkbox("Enabled", &LODLevel->bEnabled))
		{
			Viewer->MarkDirty();
			Viewer->RestartSimulation();
		}
		return;
	}

	if (UParticleEmitter* Emitter = Cast<UParticleEmitter>(SelectedObject))
	{
		ImGui::Text("LOD Count: %d", static_cast<int32>(Emitter->LODLevels.size()));
		ImGui::Text("Payload Entries: %d", static_cast<int32>(Emitter->ParticleSize.size()));
		return;
	}

	if (UParticleModuleRequired* Required = Cast<UParticleModuleRequired>(SelectedObject))
	{
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
		return;
	}

	ImGui::TextDisabled("No reflected editable properties are exposed for this module yet.");
}

void FParticleEditorViewerWidget::RenderCurveEditor(FParticleEditorViewer* Viewer)
{
	ImGui::TextUnformatted("Curve Editor");
	ImGui::SameLine();
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
		 Viewer->GetSelectionType() == EParticleEditorSelectionType::TypeDataModule ||
		 Viewer->GetSelectionType() == EParticleEditorSelectionType::Module);

	const ImVec2 CardStart = ImGui::GetCursorScreenPos();
	const float CardWidth = std::max(190.0f, ImGui::GetContentRegionAvail().x);
	const float HeaderHeight = 86.0f;
	const ImU32 AccentColor = bSelected ? IM_COL32(240, 219, 79, 255) : IM_COL32(100, 100, 100, 255);
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->AddRectFilled(CardStart, ImVec2(CardStart.x + CardWidth, CardStart.y + HeaderHeight), IM_COL32(33, 34, 38, 255), 4.0f);
	DrawList->AddRectFilled(CardStart, ImVec2(CardStart.x + 5.0f, CardStart.y + HeaderHeight), AccentColor, 2.0f);
	DrawList->AddRect(CardStart, ImVec2(CardStart.x + CardWidth, CardStart.y + HeaderHeight), IM_COL32(75, 75, 82, 255), 4.0f);

	ImGui::InvisibleButton("##EmitterHeader", ImVec2(CardWidth, HeaderHeight));
	if (ImGui::IsItemClicked())
	{
		Viewer->SelectEmitter(EmitterIndex);
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
				Viewer->CopyModuleToEmitter(DragPayload->ModuleIndex, EmitterIndex);
			}
		}
		ImGui::EndDragDropTarget();
	}

	ImGui::SetCursorScreenPos(ImVec2(CardStart.x + 14.0f, CardStart.y + 10.0f));
	ImGui::Text("Emitter %d", EmitterIndex);
	ImGui::SetCursorScreenPos(ImVec2(CardStart.x + 14.0f, CardStart.y + 40.0f));
	bool bEnabled = LOD ? LOD->bEnabled : false;
	ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(190, 45, 45, 255));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(220, 65, 65, 255));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(160, 30, 30, 255));
	if (ImGui::Button("Enable", ImVec2(72.0f, 24.0f)) && LOD)
	{
		LOD->bEnabled = !LOD->bEnabled;
		Viewer->SelectEmitter(EmitterIndex);
		Viewer->SelectLOD(LODIndex);
		Viewer->MarkDirty();
		Viewer->RestartSimulation();
	}
	ImGui::PopStyleColor(3);
	ImGui::SetCursorScreenPos(ImVec2(CardStart.x + CardWidth - 82.0f, CardStart.y + 10.0f));
	DrawEmitterPreview(ImVec2(70.0f, 58.0f), EmitterIndex, bSelected);
	ImGui::SetCursorScreenPos(ImVec2(CardStart.x, CardStart.y + HeaderHeight + 6.0f));

	if (ImGui::BeginPopupContextItem("EmitterContext"))
	{
		if (ImGui::MenuItem("Select"))
		{
			Viewer->SelectEmitter(EmitterIndex);
		}
		if (ImGui::BeginMenu("Add Module"))
		{
			Viewer->SelectEmitter(EmitterIndex);
			DrawParticleModuleClassMenu(Viewer);
			ImGui::EndMenu();
		}
		ImGui::EndPopup();
	}

	if (!LOD)
	{
		ImGui::TextDisabled("No LOD");
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
		const float RowHeight = 26.0f;
		const float RowWidth = std::max(180.0f, ImGui::GetContentRegionAvail().x);
		if ((BackgroundColor & IM_COL32_A_MASK) != 0)
		{
			ImGui::GetWindowDrawList()->AddRectFilled(
				RowStart,
				ImVec2(RowStart.x + RowWidth, RowStart.y + RowHeight),
				BackgroundColor,
				3.0f);
		}

		if (ImGui::Selectable(Label, bSelected, ImGuiSelectableFlags_SpanAvailWidth, ImVec2(0.0f, RowHeight)))
		{
			Viewer->SelectEmitter(EmitterIndex);
			Viewer->SelectLOD(LODIndex);
			switch (Type)
			{
			case EParticleEditorSelectionType::RequiredModule:
				Viewer->SelectRequiredModule();
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
