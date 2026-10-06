#pragma once

#include "Editor/UI/Panel/EditorPanelWidget.h"
#include "Editor/UI/Util/BasicReflectionPropertyRenderer.h"

class FEditorReflectionPropertyWidget : public FEditorPanelWidget
{
public:
	void Render(const FEditorPanelContext& Context) override;

private:
	FBasicReflectionPropertyRenderer Renderer;
};
