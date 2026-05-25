#pragma once

#include "Editor/UI/Viewer/EditorViewerWidget.h"
#include "Editor/Viewer/ParticleEditorViewer.h"
#include "Render/Common/ComPtr.h"

class UParticleLODLevel;
class UParticleSystem;
struct ID3D11ShaderResourceView;
struct ImDrawList;

class FParticleEditorViewerWidget : public FEditorViewerWidget
{
public:
	~FParticleEditorViewerWidget() override = default;

protected:
	void RenderContent(float DeltaTime) override;

private:
	struct FLayoutState
	{
		float EmitterPanelWidthRatio = 2.0f / 3.0f;
		float BottomPanelHeightRatio = 0.5f;
	};

	enum class ECurveEditorTool
	{
		Pan,
		Zoom
	};

	struct FCurveEditorState
	{
		EParticleEditorSelectionType Type = EParticleEditorSelectionType::None;
		int32 EmitterIndex = -1;
		int32 LODIndex = -1;
		int32 ModuleIndex = -1;
		float CanvasPanTime = 0.0f;
		float CanvasPanValue = 0.0f;
		float CanvasZoomX = 1.0f;
		float CanvasZoomY = 1.0f;
		ECurveEditorTool ActiveTool = ECurveEditorTool::Pan;

		void Clear()
		{
			Type = EParticleEditorSelectionType::None;
			EmitterIndex = -1;
			LODIndex = -1;
			ModuleIndex = -1;
		}
	};

	struct FCascadeToolbarIcons
	{
		bool bLoadAttempted = false;

		TComPtr<ID3D11ShaderResourceView> SaveIcon;
		TComPtr<ID3D11ShaderResourceView> FindIcon;
		TComPtr<ID3D11ShaderResourceView> RestartSimIcon;
		TComPtr<ID3D11ShaderResourceView> RestartLevelIcon;
		TComPtr<ID3D11ShaderResourceView> UndoIcon;
		TComPtr<ID3D11ShaderResourceView> RedoIcon;
		TComPtr<ID3D11ShaderResourceView> BoundsIcon;
		TComPtr<ID3D11ShaderResourceView> AxisIcon;
		TComPtr<ID3D11ShaderResourceView> BackgroundIcon;
		TComPtr<ID3D11ShaderResourceView> ThumbnailIcon;
		TComPtr<ID3D11ShaderResourceView> RegenLODIcon;
		TComPtr<ID3D11ShaderResourceView> LowestLODIcon;
		TComPtr<ID3D11ShaderResourceView> HighestLODIcon;
		TComPtr<ID3D11ShaderResourceView> LowerLODIcon;
		TComPtr<ID3D11ShaderResourceView> UpperLODIcon;
		TComPtr<ID3D11ShaderResourceView> AddLODIcon;
		TComPtr<ID3D11ShaderResourceView> GenericLODIcon;
		TComPtr<ID3D11ShaderResourceView> CurveHorizontalIcon;
		TComPtr<ID3D11ShaderResourceView> CurveVerticalIcon;
		TComPtr<ID3D11ShaderResourceView> CurveFitIcon;
		TComPtr<ID3D11ShaderResourceView> CurvePanIcon;
		TComPtr<ID3D11ShaderResourceView> CurveZoomIcon;
		TComPtr<ID3D11ShaderResourceView> CurveAutoIcon;
		TComPtr<ID3D11ShaderResourceView> CurveAutoClampedIcon;
		TComPtr<ID3D11ShaderResourceView> CurveUserIcon;
		TComPtr<ID3D11ShaderResourceView> CurveBreakIcon;
		TComPtr<ID3D11ShaderResourceView> CurveLinearIcon;
		TComPtr<ID3D11ShaderResourceView> CurveConstantIcon;
		TComPtr<ID3D11ShaderResourceView> CurveFlattenIcon;
		TComPtr<ID3D11ShaderResourceView> CurveStraightenIcon;
		TComPtr<ID3D11ShaderResourceView> CurveShowAllIcon;
		TComPtr<ID3D11ShaderResourceView> CurveCreateIcon;
		TComPtr<ID3D11ShaderResourceView> CurveDeleteIcon;
	};

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
	FLayoutState LayoutState;
	bool bPropertyEditUndoCaptured = false;
	TArray<int32> MultiSelectedEmitterIndices;
	TArray<int32> MultiSelectedModuleIndices;
	int32 MultiSelectedModuleEmitterIndex = -1;
	int32 MultiSelectedModuleLODIndex = -1;

	FCurveEditorState CurveState;
	FCascadeToolbarIcons ToolbarIcons;
};
