#pragma once

#include <windows.h>

#include "Core/Common.h"
#include "Editor/UI/EditorConsoleWidget.h"
#include "Editor/UI/EditorControlWidget.h"
#include "Editor/UI/EditorPropertyWidget.h"
#include "Editor/UI/EditorSceneWidget.h"
#include "Editor/UI/EditorViewportOverlayWidget.h"

class FRenderer;
class FEditorEngine;

using namespace common::structs;

class FEditorMainPanel
{
public:
	void Create(HWND InHWindow, FRenderer& InRenderer, FEditorEngine* InEditorEngine);
	void Release();
	void Render(float DeltaTime, FViewOutput& ViewOutput);
	void Update();

private:
	FEditorConsoleWidget ConsoleWidget;
	FEditorControlWidget ControlWidget;
	FEditorPropertyWidget PropertyWidget;
	FEditorSceneWidget SceneWidget;
	FEditorViewportOverlayWidget ViewportOverlayWidget;
};
