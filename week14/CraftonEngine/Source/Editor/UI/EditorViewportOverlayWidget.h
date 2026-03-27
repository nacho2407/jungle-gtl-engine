#pragma once

#include "Editor/UI/EditorWidget.h"

class FEditorViewportOverlayWidget : public FEditorWidget
{
private:
	bool bExpanded = false;

public:
	void Render(float DeltaTime) override;
};
