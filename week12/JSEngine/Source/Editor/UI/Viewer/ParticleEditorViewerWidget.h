#pragma once

#include "Editor/UI/Viewer/EditorViewerWidget.h"

class FParticleEditorViewerWidget : public FEditorViewerWidget
{
public:
	~FParticleEditorViewerWidget() override = default;

protected:
	void RenderContent(float DeltaTime) override;
};
