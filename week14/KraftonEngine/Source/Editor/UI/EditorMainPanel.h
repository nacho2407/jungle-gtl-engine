#pragma once

#include "Editor/UI/EditorConsoleWidget.h"
#include "Editor/UI/EditorControlWidget.h"
#include "Editor/UI/EditorPropertyWidget.h"
#include "Editor/UI/EditorSceneWidget.h"
#include "Editor/UI/EditorStatWidget.h"
#include "Profiling/Stats.h"

class FRenderer;
class UEditorEngine;
class FWindowsWindow;

class FEditorMainPanel
{
public:
	void Create(FWindowsWindow* InWindow, FRenderer& InRenderer, UEditorEngine* InEditorEngine);
	void Release();
	void Render(float DeltaTime);
	void Update();

private:
#if STATS
	void RenderHiZDebug(const class FEditorSettings& Settings);
#endif

	FWindowsWindow* Window = nullptr;
	UEditorEngine* EditorEngine = nullptr;
	FEditorConsoleWidget ConsoleWidget;
	FEditorControlWidget ControlWidget;
	FEditorPropertyWidget PropertyWidget;
	FEditorSceneWidget SceneWidget;
	FEditorStatWidget StatWidget;
	bool bShowWidgetList = false;
};
