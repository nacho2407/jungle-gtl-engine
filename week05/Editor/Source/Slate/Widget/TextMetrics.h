#pragma once

#include "Viewport/ViewportTypes.h"
#include <algorithm>
#include <cstdint>

namespace SWidgetTextMetrics
{
	inline bool DecodeNextCodepoint(const char* Ptr, uint32& OutCodepoint, int32& OutByteCount)
	{
		if (!Ptr || Ptr[0] == '\0')
		{
			OutCodepoint = 0;
			OutByteCount = 0;
			return false;
		}

		const unsigned char C0 = static_cast<unsigned char>(Ptr[0]);
		if (C0 < 0x80)
		{
			OutCodepoint = static_cast<uint32>(C0);
			OutByteCount = 1;
			return true;
		}

		const unsigned char C1 = static_cast<unsigned char>(Ptr[1]);
		const unsigned char C2 = static_cast<unsigned char>(Ptr[2]);
		const unsigned char C3 = static_cast<unsigned char>(Ptr[3]);

		if ((C0 & 0xE0) == 0xC0 && (C1 & 0xC0) == 0x80)
		{
			OutCodepoint = static_cast<uint32>((C0 & 0x1F) << 6) |
				static_cast<uint32>(C1 & 0x3F);
			OutByteCount = 2;
			return true;
		}

		if ((C0 & 0xF0) == 0xE0 && (C1 & 0xC0) == 0x80 && (C2 & 0xC0) == 0x80)
		{
			OutCodepoint =
				(static_cast<uint32>(C0 & 0x0F) << 12) |
				(static_cast<uint32>(C1 & 0x3F) << 6) |
				static_cast<uint32>(C2 & 0x3F);
			OutByteCount = 3;
			return true;
		}

		if ((C0 & 0xF8) == 0xF0 && (C1 & 0xC0) == 0x80 && (C2 & 0xC0) == 0x80 && (C3 & 0xC0) == 0x80)
		{
			OutCodepoint =
				(static_cast<uint32>(C0 & 0x07) << 18) |
				(static_cast<uint32>(C1 & 0x3F) << 12) |
				(static_cast<uint32>(C2 & 0x3F) << 6) |
				static_cast<uint32>(C3 & 0x3F);
			OutByteCount = 4;
			return true;
		}

		OutCodepoint = static_cast<uint32>('?');
		OutByteCount = 1;
		return true;
	}

	inline float ResolveAdvanceUnit(uint32 Codepoint)
	{
		if (Codepoint == static_cast<uint32>(' '))
		{
			return 0.35f;
		}

		if (Codepoint == static_cast<uint32>('\t'))
		{
			return 1.40f;
		}

		if (Codepoint <= 0x007E)
		{
			return 0.82f;
		}

		return 1.00f;
	}

	inline bool ProducesGlyphQuad(uint32 Codepoint)
	{
		if (Codepoint == static_cast<uint32>(' ') || Codepoint == static_cast<uint32>('\t'))
		{
			return false;
		}

		if (Codepoint < 0x20)
		{
			return false;
		}

		return true;
	}

	inline size_t PrevUtf8PrefixLength(const FString& Text, size_t PrefixLength)
	{
		if (Text.empty() || PrefixLength == 0)
		{
			return 0;
		}

		size_t Length = (std::min)(PrefixLength, Text.size());
		--Length;

		// Move to the lead byte of the previous UTF-8 codepoint.
		while (Length > 0 && (static_cast<unsigned char>(Text[Length]) & 0xC0) == 0x80)
		{
			--Length;
		}

		return Length;
	}

	inline FVector2 MeasureText(const char* Text, float FontSize, float LetterSpacing)
	{
		if (!Text || Text[0] == '\0' || FontSize <= 0.0f)
		{
			return { 0.0f, 0.0f };
		}

		const float SafeFontSize = (std::max)(FontSize, 1.0f);
		const float SpacingScale = (std::max)(LetterSpacing, 0.0f);
		const float LineHeight = SafeFontSize;

		float CurrentPenX = 0.0f;
		float CurrentLineMaxX = 0.0f;
		float MaxLineWidth = 0.0f;
		bool bLineHasGlyph = false;
		int32 LineCount = 1;

		const auto FinalizeLine = [&]()
			{
				const float LineWidth = bLineHasGlyph ? (std::max)(CurrentLineMaxX, CurrentPenX) : CurrentPenX;
				MaxLineWidth = (std::max)(MaxLineWidth, LineWidth);
				CurrentPenX = 0.0f;
				CurrentLineMaxX = 0.0f;
				bLineHasGlyph = false;
			};

		for (size_t Index = 0; Text[Index] != '\0';)
		{
			uint32 Codepoint = 0;
			int32 ByteCount = 0;
			if (!DecodeNextCodepoint(Text + Index, Codepoint, ByteCount) || ByteCount <= 0)
			{
				break;
			}

			if (Codepoint == static_cast<uint32>('\n'))
			{
				FinalizeLine();
				++LineCount;
				Index += static_cast<size_t>(ByteCount);
				continue;
			}

			const float Advance = ResolveAdvanceUnit(Codepoint) * SafeFontSize * SpacingScale;
			if (ProducesGlyphQuad(Codepoint))
			{
				const float GlyphRight = CurrentPenX + SafeFontSize;
				CurrentLineMaxX = (std::max)(CurrentLineMaxX, GlyphRight);
				bLineHasGlyph = true;
			}

			CurrentPenX += Advance;
			Index += static_cast<size_t>(ByteCount);
		}

		FinalizeLine();
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

	inline float MeasureTextLogicalWidth(const char* Text, float FontSize, float LetterSpacing)
	{
		if (!Text || Text[0] == '\0' || FontSize <= 0.0f)
		{
			return 0.0f;
		}

		const float SafeFontSize = (std::max)(FontSize, 1.0f);
		const float SpacingScale = (std::max)(LetterSpacing, 0.0f);

		float CurrentPenX = 0.0f;
		float MaxLineWidth = 0.0f;

		const auto FinalizeLine = [&]()
			{
				MaxLineWidth = (std::max)(MaxLineWidth, CurrentPenX);
				CurrentPenX = 0.0f;
			};

		for (size_t Index = 0; Text[Index] != '\0';)
		{
			uint32 Codepoint = 0;
			int32 ByteCount = 0;
			if (!DecodeNextCodepoint(Text + Index, Codepoint, ByteCount) || ByteCount <= 0)
			{
				break;
			}

			if (Codepoint == static_cast<uint32>('\n'))
			{
				FinalizeLine();
				Index += static_cast<size_t>(ByteCount);
				continue;
			}

			CurrentPenX += ResolveAdvanceUnit(Codepoint) * SafeFontSize * SpacingScale;
			Index += static_cast<size_t>(ByteCount);
		}

		FinalizeLine();
		return MaxLineWidth;
	}

	inline float MeasureTextLogicalWidth(const FString& Text, float FontSize, float LetterSpacing)
	{
		return MeasureTextLogicalWidth(Text.c_str(), FontSize, LetterSpacing);
	}
}
