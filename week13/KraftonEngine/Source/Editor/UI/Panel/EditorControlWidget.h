#pragma once

#include "Editor/UI/Panel/EditorPanelWidget.h"

class FEditorControlWidget : public FEditorPanelWidget
{
public:
	virtual void Render(const FEditorPanelContext& Context) override;
};
