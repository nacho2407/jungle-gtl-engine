#pragma once

#include "Editor/UI/Viewer/EditorViewerWidget.h"

class FParticleEditorViewer;
class UParticleSystem;

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
	void DrawModuleNode(FParticleEditorViewer* Viewer, int32 ModuleIndex);
};
