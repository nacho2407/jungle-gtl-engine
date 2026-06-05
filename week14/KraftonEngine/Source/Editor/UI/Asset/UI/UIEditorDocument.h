#pragma once

#include "Core/Types/CoreTypes.h"

#include <filesystem>

struct FSourceRange
{
	int32 Begin = -1;
	int32 End = -1;

	bool IsValid() const
	{
		return Begin >= 0 && End >= Begin;
	}
};

struct FStyleValueSpan
{
	FString PropertyName;
	FString OriginalValue;
	FSourceRange ValueRange;
	bool bExistsInSource = false;
};

struct FUIEditorTextElement
{
	FString TagName;
	FString Id;
	FString ClassName;
	FString Text;
	bool bSelfClosing = false;

	float X = 0.0f;
	float Y = 0.0f;
	float Width = 200.0f;
	float Height = 40.0f;
	bool bUsePercentLayout = false;

	float FontSize = 24.0f;
	FString FontWeight = "normal";
	float Color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

	FSourceRange OpenTagRange;
	FSourceRange ElementRange;
	FSourceRange InnerTextRange;
	FSourceRange StyleAttributeValueRange;

	FStyleValueSpan LeftStyle;
	FStyleValueSpan PositionStyle;
	FStyleValueSpan TopStyle;
	FStyleValueSpan WidthStyle;
	FStyleValueSpan HeightStyle;
	FStyleValueSpan FontSizeStyle;
	FStyleValueSpan FontWeightStyle;
	FStyleValueSpan ColorStyle;

	bool bCanEditText = true;
	bool bTextDirty = false;
	bool bStyleDirty = false;
	bool bPendingInsert = false;
};

struct FUIEditorDocument
{
	std::filesystem::path SourcePath;
	std::filesystem::path DraftPath;
	TArray<FUIEditorTextElement> TextElements;
	TArray<FSourceRange> DeletedElementRanges;
	FString OriginalSource;
	FString CurrentSource;
	bool bDirty = false;
};
