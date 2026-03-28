#pragma once

#include "Core/CoreTypes.h"

class UEditorEngine;

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

	TArray<FString> BuildLines(const UEditorEngine& Editor) const;

private:
	bool bShowFPS = false;
	bool bShowMemory = false;
};