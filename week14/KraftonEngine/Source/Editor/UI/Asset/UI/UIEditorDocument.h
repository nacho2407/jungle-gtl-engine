#pragma once

#include "Core/Types/CoreTypes.h"

#include <filesystem>

struct FUIEditorTextElement
{
	FString Id;
	FString Text;

	float X = 0.0f;
	float Y = 0.0f;
	float Width = 200.0f;
	float Height = 40.0f;

	float FontSize = 24.0f;
	FString FontWeight = "normal";
};

struct FUIEditorDocument
{
	std::filesystem::path SourcePath;
	TArray<FUIEditorTextElement> TextElements;
	FString OriginalRml;
	bool bDirty = false;
};
