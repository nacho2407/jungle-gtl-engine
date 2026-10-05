#include "ParticleEditorInternal.h"

using namespace ParticleEditorInternal;

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
	const bool bCurveKeyOwnsDelete = CurveState.bWantsDeleteKeyFocus && CurveState.SelectedKeyIndex >= 0;
	HandleParticleEditorShortcuts(ParticleViewer, !bCurveKeyOwnsDelete);

	const float SplitterThickness = 4.0f;
	const float SplitterSideGap = 6.0f;
	const float SplitterTotalWidth = SplitterThickness + SplitterSideGap * 2.0f;

	const float MinColumnWidth = std::min(220.0f, std::max(80.0f, (FullSize.x - SplitterThickness) * 0.25f));
	const float MinPanelHeight = std::min(140.0f, std::max(60.0f, (FullSize.y - SplitterThickness) * 0.25f));
	LayoutState.EmitterPanelWidthRatio = std::clamp(LayoutState.EmitterPanelWidthRatio, 0.2f, 0.85f);
	LayoutState.BottomPanelHeightRatio = std::clamp(LayoutState.BottomPanelHeightRatio, 0.2f, 0.8f);

	float RightWidth = FullSize.x * LayoutState.EmitterPanelWidthRatio;
	RightWidth = std::clamp(RightWidth, MinColumnWidth, std::max(MinColumnWidth, FullSize.x - MinColumnWidth - SplitterThickness));
	float BottomHeight = FullSize.y * LayoutState.BottomPanelHeightRatio;
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
		LayoutState.BottomPanelHeightRatio = BottomHeight / FullSize.y;
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
		LayoutState.EmitterPanelWidthRatio = RightWidth / FullSize.x;
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
		LayoutState.BottomPanelHeightRatio = BottomHeight / FullSize.y;
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
			if (ImGui::MenuItem("Save", "Ctrl+S", false, Viewer->GetParticleSystem() != nullptr))
			{
				Viewer->Save();
			}
			if (ImGui::MenuItem("Save As", nullptr, false, Viewer->GetParticleSystem() != nullptr))
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
			float EmitterRatio = LayoutState.EmitterPanelWidthRatio;
			if (ImGui::SliderFloat("Emitter Width", &EmitterRatio, 0.2f, 0.85f, "%.2f"))
			{
				LayoutState.EmitterPanelWidthRatio = EmitterRatio;
			}
			float BottomRatio = LayoutState.BottomPanelHeightRatio;
			if (ImGui::SliderFloat("Bottom Height", &BottomRatio, 0.2f, 0.8f, "%.2f"))
			{
				LayoutState.BottomPanelHeightRatio = BottomRatio;
			}
			if (ImGui::MenuItem("Reset Particle Layout"))
			{
				LayoutState.EmitterPanelWidthRatio = 2.0f / 3.0f;
				LayoutState.BottomPanelHeightRatio = 0.5f;
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

