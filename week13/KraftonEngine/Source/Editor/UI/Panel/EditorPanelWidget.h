#pragma once

#include "Editor/UI/EditorWidget.h"

class FEditorSettings;
class UObject;
class UStruct;
class UEditorEngine;

struct FSelectionDetailTarget
{
	UObject* ObjectPtr = nullptr;
	UStruct* StructType = nullptr;
	void* ContainerPtr = nullptr;

	void Reset()
	{
		ObjectPtr = nullptr;
		StructType = nullptr;
		ContainerPtr = nullptr;
	}

	bool HasTarget() const
	{
		return StructType != nullptr && ContainerPtr != nullptr;
	}
};

struct FEditorPanelContext
{
	UEditorEngine* EditorEngine = nullptr;
	FSelectionDetailTarget SelectionDetailTarget;
	float DeltaTime = 0.0f;
	const FEditorSettings* Settings = nullptr;
	bool bHideEditorWindows = false;
};

class FEditorPanelWidget : public FEditorWidget
{
public:
	void Render(float DeltaTime) final override
	{
		FallbackContext.DeltaTime = DeltaTime;
		Render(FallbackContext);
	}

	virtual void Render(const FEditorPanelContext& Context) = 0;

private:
	FEditorPanelContext FallbackContext;
};
