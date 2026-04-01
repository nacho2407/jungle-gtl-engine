#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"
#include "Render/Pipeline/RenderCommand.h"

class UEditorEngine;

struct FOverlayStatGroup
{
	TArray<FString> Lines;
};

struct FOverlayStatLayout
{
	float StartX = 16.0f;
	float StartY = 25.0f;
	float LineHeight = 20.0f;
	float GroupSpacing = 12.0f;
	float TextScale = 1.0f;
};

class FOverlayStatSystem
{
public:
	void ShowFPS(bool bEnable = true) { bShowFPS = bEnable; }
	void ShowMemory(bool bEnable = true) { bShowMemory = bEnable; }
	void HideAll()
	{
		bShowFPS = false;
		bShowMemory = false;
	}

	const FOverlayStatLayout& GetLayout() const { return Layout; }
	FOverlayStatLayout& GetLayout() { return Layout; }

	TArray<FOverlayStatGroup> BuildGroups(const UEditorEngine& Editor) const;
	TArray<FOverlayStatLine> BuildLines(const UEditorEngine& Editor) const;

private:
	bool bShowFPS = false;
	bool bShowMemory = false;

	FOverlayStatLayout Layout;
};