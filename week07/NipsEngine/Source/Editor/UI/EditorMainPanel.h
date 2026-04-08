#pragma once

#include "ImGui/imgui.h"
#include "Editor/UI/EditorConsoleWidget.h"
#include "Editor/UI/EditorControlWidget.h"
#include "Editor/UI/EditorMaterialWidget.h"
#include "Editor/UI/EditorPropertyWidget.h"
#include "Editor/UI/EditorSceneWidget.h"
#include "Editor/UI/EditorViewportOverlayWidget.h"
#include "Editor/UI/EditorStatWidget.h"
#include "Editor/UI/EditorToolbarWidget.h"

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
	void RenderViewportHostWindow();
	void RenderViewportMenuBarForIndex(int32 ViewportIndex);
private:
	FWindowsWindow* Window = nullptr;
	UEditorEngine* EditorEngine = nullptr;

	ImVector<ImWchar> FontGlyphRanges; // 폰트 아틀라스 빌드 전까지 수명 유지 필요
	FEditorConsoleWidget ConsoleWidget;
	FEditorControlWidget ControlWidget;
	FEditorPropertyWidget PropertyWidget;
	FEditorSceneWidget SceneWidget;
	FEditorMaterialWidget MaterialWidget;
	FEditorViewportOverlayWidget ViewportOverlayWidget;
	FEditorStatWidget StatWidget;
	FEditorToolbarWidget ToolbarWidget;

	bool bShowConsole = true;
	bool bShowControl = true;
	bool bShowProperty = true;
	bool bShowSceneManager = true;
	bool bShowMaterialEditor = true;
	bool bShowStatProfiler = true;
};
