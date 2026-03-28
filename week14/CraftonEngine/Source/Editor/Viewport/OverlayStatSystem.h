#pragma once

#include "Core/CoreTypes.h"

class UEditorEngine;

struct FOverlayStatGroup
{
	float StartY;
	TArray<FString> Lines;
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

	bool IsFPSVisible() const { return bShowFPS; }
	bool IsMemoryVisible() const { return bShowMemory; }
	bool HasAnyVisible() const { return bShowFPS || bShowMemory; }

	TArray<FOverlayStatGroup> BuildGroups(const UEditorEngine& Editor) const;

private:
	bool bShowFPS = false;
	bool bShowMemory = false;

	static constexpr float FPSStartY = 25.0f;
	static constexpr float MemoryStartY = 25.0f + 16.0f * 4; // FPS 아래에 위치하도록 간격 조정
};