#pragma once

#include "CoreMinimal.h"
#include <functional>
#include <Windows.h>

class FEngine;
class FEditorEngine;
class FEditorViewportRegistry;
class FPicker;
class FGizmo;

class FEditorViewportInputService
{
public:
	void TickCameraNavigation(
		FEngine* Engine,
		FEditorEngine* EditorEngine,
		FEditorViewportRegistry& ViewportRegistry,
		const FGizmo& Gizmo);

	void HandleMessage(
		FEngine* Engine,
		FEditorEngine* EditorEngine,
		HWND Hwnd,
		UINT Msg,
		WPARAM WParam,
		LPARAM LParam,
		FEditorViewportRegistry& ViewportRegistry,
		FPicker& Picker,
		FGizmo& Gizmo,
		const std::function<void()>& OnSelectionChanged);

	int32 GetScreenMouseX() const { return ScreenMouseX; }
	int32 GetScreenMouseY() const { return ScreenMouseY; }

private:
	int32 ScreenWidth = 0;
	int32 ScreenHeight = 0;
	int32 ScreenMouseX = 0;
	int32 ScreenMouseY = 0;
};
