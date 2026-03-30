#pragma once

#include "Editor/UI/EditorWidget.h"

class FEditorViewportOverlayWidget : public FEditorWidget
{
private:
	bool bExpanded = false;
	void RenderViewportSettings(float DeltaTime);
    void RenderDebugStats(float DeltaTime);

public:
	void Render(float DeltaTime) override;
};
