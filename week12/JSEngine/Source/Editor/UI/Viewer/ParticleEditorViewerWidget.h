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
	void RenderToolbar(FParticleEditorViewer* ParticleViewer);
	void RenderViewportPanels(FParticleEditorViewer* ParticleViewer);
	void RenderEmitterPanels(FParticleEditorViewer* ParticleViewer, UParticleSystem* ParticleSystem);
	void RenderDetailPanels(FParticleEditorViewer* ParticleViewer);
	void RenderCurveEditor(FParticleEditorViewer* ParticleViewer);
};
