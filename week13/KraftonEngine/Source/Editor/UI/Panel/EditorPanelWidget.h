#pragma once

#include "Editor/UI/EditorWidget.h"

class FEditorSettings;
class FSelectionManager;
class UEditorEngine;

struct FEditorPanelContext
{
	UEditorEngine* EditorEngine = nullptr;
	FSelectionManager* SelectionManager = nullptr;
	float DeltaTime = 0.0f;
	const FEditorSettings* Settings = nullptr;
	bool bHideEditorWindows = false;
};

class FEditorPanelWidget : public FEditorWidget
{
public:
	void Render(float DeltaTime) final override
	{
		FEditorPanelContext Context;
		Context.DeltaTime = DeltaTime;
		Render(Context);
	}

	virtual void Render(const FEditorPanelContext& Context) = 0;
};
