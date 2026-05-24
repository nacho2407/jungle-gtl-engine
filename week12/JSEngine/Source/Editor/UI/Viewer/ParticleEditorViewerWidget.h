#pragma once

#include "Editor/UI/Viewer/EditorViewerWidget.h"
#include "Render/Common/ComPtr.h"

class FParticleEditorViewer;
class UParticleSystem;
struct ID3D11ShaderResourceView;

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
	void DrawLODNode(FParticleEditorViewer* Viewer, int32 EmitterIndex, int32 LODIndex);
	void DrawModuleNode(FParticleEditorViewer* Viewer, int32 EmitterIndex, int32 LODIndex, int32 ModuleIndex);

	void LoadCascadeToolbarIcons();
	bool DrawCascadeToolbarIconButton(const char* Id, ID3D11ShaderResourceView* Icon, const char* Tooltip, const ImVec2& Size, bool bEnabled = true, const char* Label = nullptr);

private:
	float EmitterPanelWidthRatio = 2.0f / 3.0f;
	float BottomPanelHeightRatio = 0.5f;

	int32 CurveSourceModuleIndex = -1;
	bool bCascadeToolbarIconsLoadAttempted = false;

	TComPtr<ID3D11ShaderResourceView> CascadeSaveIcon;
	TComPtr<ID3D11ShaderResourceView> CascadeFindIcon;
	TComPtr<ID3D11ShaderResourceView> CascadeRestartSimIcon;
	TComPtr<ID3D11ShaderResourceView> CascadeRestartLevelIcon;
	TComPtr<ID3D11ShaderResourceView> CascadeUndoIcon;
	TComPtr<ID3D11ShaderResourceView> CascadeRedoIcon;
	TComPtr<ID3D11ShaderResourceView> CascadeBoundsIcon;
	TComPtr<ID3D11ShaderResourceView> CascadeAxisIcon;
	TComPtr<ID3D11ShaderResourceView> CascadeBackgroundIcon;
	TComPtr<ID3D11ShaderResourceView> CascadeThumbnailIcon;
	TComPtr<ID3D11ShaderResourceView> CascadeRegenLODIcon;
	TComPtr<ID3D11ShaderResourceView> CascadeLowestLODIcon;
	TComPtr<ID3D11ShaderResourceView> CascadeHighestLODIcon;
	TComPtr<ID3D11ShaderResourceView> CascadeLowerLODIcon;
	TComPtr<ID3D11ShaderResourceView> CascadeUpperLODIcon;
	TComPtr<ID3D11ShaderResourceView> CascadeAddLODIcon;
	TComPtr<ID3D11ShaderResourceView> CascadeGenericLODIcon;
};
