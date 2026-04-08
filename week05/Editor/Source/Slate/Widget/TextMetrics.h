#pragma once

#include "Viewport/ViewportTypes.h"
#include <algorithm>
#include <cctype>

namespace SWidgetTextMetrics
{
	inline float EstimateGlyphAdvanceUnit(const char* Ptr, int32& OutByteCount)
	{
		const unsigned char C = static_cast<unsigned char>(Ptr[0]);
		if (C < 0x80)
		{
			OutByteCount = 1;

			if (C == static_cast<unsigned char>(' '))
			{
				return 0.33f;
			}

			if (C == static_cast<unsigned char>('\t'))
			{
				return 1.30f;
			}

			if (std::isdigit(C))
			{
				return 0.58f;
			}

			if (std::ispunct(C))
			{
				return 0.48f;
			}

			return 0.62f;
		}

		if ((C & 0xE0) == 0xC0)
		{
			OutByteCount = 2;
			return 0.92f;
		}

		if ((C & 0xF0) == 0xE0)
		{
			OutByteCount = 3;
			return 1.00f;
		}

		if ((C & 0xF8) == 0xF0)
		{
			OutByteCount = 4;
			return 1.05f;
		}

		OutByteCount = 1;
		return 0.62f;
	}

	inline FVector2 MeasureText(const char* Text, float FontSize, float LetterSpacing)
	{
		if (!Text || Text[0] == '\0' || FontSize <= 0.0f)
		{
			return { 0.0f, 0.0f };
		}

		const float SafeFontSize = (std::max)(FontSize, 1.0f);
		const float SpacingScale = (std::max)(LetterSpacing, 0.0f);
		const float LineHeight = SafeFontSize * 1.2f;

		float CurrentLineWidth = 0.0f;
		float MaxLineWidth = 0.0f;
		int32 LineCount = 1;

		for (size_t Index = 0; Text[Index] != '\0';)
		{
			const unsigned char C = static_cast<unsigned char>(Text[Index]);
			if (C == static_cast<unsigned char>('\n'))
			{
				MaxLineWidth = (std::max)(MaxLineWidth, CurrentLineWidth);
				CurrentLineWidth = 0.0f;
				++LineCount;
				++Index;
				continue;
			}

			int32 ByteCount = 1;
			const float AdvanceUnit = EstimateGlyphAdvanceUnit(Text + Index, ByteCount);
			CurrentLineWidth += AdvanceUnit * SafeFontSize * SpacingScale;
			Index += static_cast<size_t>(ByteCount);
		}

		MaxLineWidth = (std::max)(MaxLineWidth, CurrentLineWidth);
		return { MaxLineWidth, LineHeight * static_cast<float>(LineCount) };
	}

	inline FVector2 MeasureText(const FString& Text, float FontSize, float LetterSpacing)
	{
		return MeasureText(Text.c_str(), FontSize, LetterSpacing);
	}

	inline float MeasureTextWidth(const FString& Text, float FontSize, float LetterSpacing)
	{
		return MeasureText(Text, FontSize, LetterSpacing).X;
	}
}
