#pragma once

#include "Core/Types/CoreTypes.h"
#include "Editor/UI/Panel/EditorPanelWidget.h"

class EditorProjectSettingsWidget : public FEditorPanelWidget
{
public:
	void Render(const FEditorPanelContext& Context) override;

	bool bOpen = false;
};
