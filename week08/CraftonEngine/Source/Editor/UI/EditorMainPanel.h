#pragma once

#include "Editor/UI/EditorConsoleWidget.h"
#include "Editor/UI/EditorControlWidget.h"
#include "Editor/UI/EditorPropertyWidget.h"
#include "Editor/UI/EditorSceneWidget.h"
#include "Editor/UI/EditorViewportOverlayWidget.h"
#include "Editor/UI/EditorViewportWidget.h"
#include "Editor/UI/EditorStatWidget.h"

class FRenderer;
class UEditorEngine;
class FWindowsWindow;

class FEditorMainPanel
{
public:
	static constexpr int32 MaxViewports = 4;

	void Create(FWindowsWindow* InWindow, FRenderer& InRenderer, UEditorEngine* InEditorEngine);
	void Release();
	void Render(float DeltaTime);
	void Update();

	FEditorViewportWidget& GetViewportWidget(int32 Index = 0) { return ViewportWidgets[Index]; }
	void SetActiveViewportCount(int32 Count) { ActiveViewportCount = Count; }
	int32 GetActiveViewportCount() const { return ActiveViewportCount; }

private:
	FWindowsWindow* Window = nullptr;
	FEditorConsoleWidget ConsoleWidget;
	FEditorControlWidget ControlWidget;
	FEditorPropertyWidget PropertyWidget;
	FEditorSceneWidget SceneWidget;
	FEditorViewportOverlayWidget ViewportOverlayWidget;
	FEditorViewportWidget ViewportWidgets[MaxViewports];
	FEditorStatWidget StatWidget;

	int32 ActiveViewportCount = 1;
};
