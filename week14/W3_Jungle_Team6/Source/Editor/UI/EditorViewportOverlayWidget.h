#pragma once

#include "Editor/UI/EditorWidget.h"

class FEditorViewportOverlayWidget : public FEditorWidget
{
public:
	void Render(float DeltaTime, FViewOutput& ViewOutput) override;
};
