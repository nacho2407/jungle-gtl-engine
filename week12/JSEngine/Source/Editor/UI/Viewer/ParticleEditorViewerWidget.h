#pragma once

#include "Editor/UI/Viewer/EditorViewerWidget.h"
#include "Editor/Viewer/ParticleEditorViewer.h"
#include "ParticleEditorCurvePanel.h"
#include "ParticleEditorToolbar.h"

class UParticleLODLevel;
class UParticleSystem;
struct ID3D11ShaderResourceView;
struct ImDrawList;

struct FParticleEditorLayoutState
{
	float EmitterPanelWidthRatio = 2.0f / 3.0f;
	float BottomPanelHeightRatio = 0.5f;
};

class FParticleEditorViewerWidget : public FEditorViewerWidget
{
public:
	~FParticleEditorViewerWidget() override = default;

protected:
	void RenderContent(float DeltaTime) override;

private:
	void RenderMenuBar(FParticleEditorViewer* Viewer);
	void RenderToolbar(FParticleEditorViewer* Viewer);
	void RenderViewportOptions(FParticleEditorViewer* Viewer);
	void RenderTimeControls(FParticleEditorViewer* Viewer);
	void RenderEmitterPanel(FParticleEditorViewer* Viewer);
	void RenderEmitterContextMenu(FParticleEditorViewer* Viewer);
	void RenderDetailsPanel(FParticleEditorViewer* Viewer);
	void RenderCurveEditor(FParticleEditorViewer* Viewer);

	void DrawEmitterNode(FParticleEditorViewer* Viewer, int32 EmitterIndex);
	void DrawEmitterNodeHeader(FParticleEditorViewer* Viewer, UParticleLODLevel* LOD, int32 EmitterIndex, int32 LODIndex, const ImVec2& CardStart, float CardWidth, float HeaderHeight, float HeaderPreviewSize, bool bSelected);
	void DrawEmitterNodeModuleList(FParticleEditorViewer* Viewer, UParticleLODLevel* LOD, int32 EmitterIndex, int32 LODIndex, const ImVec2& CardStart, float CardWidth, float HeaderHeight, float SeparatorBottom);
	void DrawEmitterNodeSelectionOutline(const ImVec2& CardStart, float CardWidth, float SeparatorBottom, bool bSelected, ImDrawList* BaseDrawList);
	void DrawLODNode(FParticleEditorViewer* Viewer, int32 EmitterIndex, int32 LODIndex);
	void DrawModuleNode(FParticleEditorViewer* Viewer, int32 EmitterIndex, int32 LODIndex, int32 ModuleIndex);

	void LoadCascadeToolbarIcons();
	bool DrawCascadeToolbarIconButton(const char* Id, ID3D11ShaderResourceView* Icon, const char* Tooltip, const ImVec2& Size, bool bEnabled = true, const char* Label = nullptr);

private:
	FParticleEditorLayoutState LayoutState;
	bool bPropertyEditUndoCaptured = false;
	TArray<int32> MultiSelectedEmitterIndices;
	TArray<int32> MultiSelectedModuleIndices;
	int32 MultiSelectedModuleEmitterIndex = -1;
	int32 MultiSelectedModuleLODIndex = -1;

	FParticleCurveEditorState CurveState;
	FCascadeToolbarIcons ToolbarIcons;
};
