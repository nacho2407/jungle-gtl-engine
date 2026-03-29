#pragma once

#include "CoreMinimal.h"

class FEditorUI;
class FEditorViewportRegistry;
class FPicker;

class FEditorViewportAssetInteractionService
{
public:
	void HandleFileDoubleClick(
		FEditorUI& EditorUI,
		FEditorViewportRegistry& ViewportRegistry,
		const FString& FilePath) const;

	void HandleFileDropOnViewport(
		FEditorUI& EditorUI,
		const FPicker& Picker,
		const FEditorViewportRegistry& ViewportRegistry,
		int32 ScreenMouseX,
		int32 ScreenMouseY,
		const FString& FilePath) const;
};
