#include "Text/Test.h"

#include "Text/FontAsset.h"
#include "Text/TextEncoding.h"

#include <Windows.h>
#include <cstdio>
#include <iomanip>
#include <iostream>

namespace
{
	const char* DEFAULT_SAMPLE_TEXT = "A B";

	struct FTestContext
	{
		int32 Passed = 0;
		int32 Failed = 0;
	};

	void ReportCheck(FTestContext& Context, bool bCondition, const char* Message)
	{
		if (bCondition)
		{
			++Context.Passed;
			std::cout << "[PASS] " << Message << "\n";
		}
		else
		{
			++Context.Failed;
			std::cout << "[FAIL] " << Message << "\n";
		}
	}

	bool IsUvInRange(float Value)
	{
		return Value >= 0.0f && Value <= 1.0f;
	}

	void PrintUtf8Bytes(const FString& Text)
	{
		std::cout << "Input bytes: ";
		for (uint8 Byte : Text)
		{
			std::cout
				<< std::uppercase
				<< std::hex
				<< std::setw(2)
				<< std::setfill('0')
				<< static_cast<uint32>(Byte)
				<< ' ';
		}
		std::cout << std::dec << std::setfill(' ') << "\n";
	}

	void PrintCodepoints(const FU32String& Codepoints)
	{
		std::cout << "Codepoints(" << Codepoints.size() << "): ";
		for (char32_t Codepoint : Codepoints)
		{
			std::cout
				<< "U+"
				<< std::uppercase
				<< std::hex
				<< static_cast<uint32>(Codepoint)
				<< ' ';
		}
		std::cout << std::dec << "\n";
	}

	void PrintGlyphSummary(uint32 Codepoint, const FGlyphInfo& Glyph)
	{
		std::cout
			<< "  U+"
			<< std::uppercase
			<< std::hex
			<< Codepoint
			<< std::dec
			<< " Rect=(" << Glyph.AtlasRect.X << ", " << Glyph.AtlasRect.Y
			<< ", " << Glyph.AtlasRect.Width << ", " << Glyph.AtlasRect.Height << ")"
			<< " UV=(" << std::fixed << std::setprecision(4)
			<< Glyph.U0 << ", " << Glyph.V0 << ")-("
			<< Glyph.U1 << ", " << Glyph.V1 << ")"
			<< " AdvanceX=" << Glyph.Metrics.AdvanceX
			<< " Bearing=(" << Glyph.Metrics.BearingX << ", " << Glyph.Metrics.BearingY << ")"
			<< " Size=(" << Glyph.Metrics.Width << ", " << Glyph.Metrics.Height << ")\n";
		std::cout.unsetf(std::ios::floatfield);
	}

	void RunGlyphAtlasTests(FTestContext& Context, const FString& SampleText)
	{
		std::cout << "[GlyphAtlas / FontAsset]\n";

		const FString FontPath = "Assets/Fonts/D2CodingSubset.ttf";
		const FString DumpPath = "TextAtlasDebug.pgm";

		FFontAsset FontAsset;
		const bool bLoaded = FontAsset.LoadFromFile(FontPath, 32.0f);
		ReportCheck(Context, bLoaded, "Load D2Coding font");
		if (!bLoaded)
		{
			return;
		}

		std::cout << "Input text: " << SampleText << "\n";
		PrintUtf8Bytes(SampleText);

		const FU32String Codepoints = Utf8ToCodepoints(SampleText);
		ReportCheck(Context, !Codepoints.empty(), "UTF-8 decoding produced at least one codepoint");
		PrintCodepoints(Codepoints);

		for (char32_t CodepointChar : Codepoints)
		{
			const uint32 Codepoint = static_cast<uint32>(CodepointChar);

			FGlyphInfo* Glyph = FontAsset.EnsureGlyph(Codepoint);
			ReportCheck(Context, Glyph != nullptr, "EnsureGlyph returns glyph or fallback");
			if (Glyph == nullptr)
			{
				continue;
			}

			const FGlyphInfo* FoundGlyph = FontAsset.FindGlyph(Codepoint);
			FGlyphInfo* CachedGlyph = FontAsset.EnsureGlyph(Codepoint);
			ReportCheck(Context, CachedGlyph == Glyph, "EnsureGlyph returns same cached entry");

			const bool bUsedFallback = (Glyph->Codepoint != Codepoint);
			const bool bBlankGlyph = (!bUsedFallback && Glyph->AtlasRect.Width == 0.0f && Glyph->AtlasRect.Height == 0.0f);

			if (!bUsedFallback)
			{
				ReportCheck(Context, FoundGlyph == Glyph, "FindGlyph returns cached glyph");
			}
			else
			{
				ReportCheck(Context, true, "Unsupported glyph fell back to replacement glyph");
			}

			ReportCheck(Context, Glyph->Metrics.AdvanceX > 0.0f, "Glyph advance is positive");
			ReportCheck(Context, Glyph->Metrics.Width >= 0.0f, "Glyph metric width is non-negative");
			ReportCheck(Context, Glyph->Metrics.Height >= 0.0f, "Glyph metric height is non-negative");

			if (bBlankGlyph)
			{
				ReportCheck(Context, true, "Blank glyph stored as metrics-only entry");
				ReportCheck(Context, Glyph->U0 == 0.0f && Glyph->V0 == 0.0f, "Blank glyph UV start is zero");
				ReportCheck(Context, Glyph->U1 == 0.0f && Glyph->V1 == 0.0f, "Blank glyph UV end is zero");
			}
			else
			{
				ReportCheck(Context, Glyph->AtlasRect.Width > 0.0f, "Rendered glyph atlas rect width is positive");
				ReportCheck(Context, Glyph->AtlasRect.Height > 0.0f, "Rendered glyph atlas rect height is positive");
				ReportCheck(Context, Glyph->AtlasRect.X >= 0.0f, "Atlas rect X is non-negative");
				ReportCheck(Context, Glyph->AtlasRect.Y >= 0.0f, "Atlas rect Y is non-negative");
				ReportCheck(Context, Glyph->U0 < Glyph->U1, "Glyph UV U range is valid");
				ReportCheck(Context, Glyph->V0 < Glyph->V1, "Glyph UV V range is valid");
				ReportCheck(Context, IsUvInRange(Glyph->U0), "Glyph U0 is within atlas UV range");
				ReportCheck(Context, IsUvInRange(Glyph->V0), "Glyph V0 is within atlas UV range");
				ReportCheck(Context, IsUvInRange(Glyph->U1), "Glyph U1 is within atlas UV range");
				ReportCheck(Context, IsUvInRange(Glyph->V1), "Glyph V1 is within atlas UV range");
			}

			PrintGlyphSummary(Codepoint, *Glyph);
		}

		const bool bDumped = FontAsset.GetAtlas().SaveDebugPGM(DumpPath);
		ReportCheck(Context, bDumped, "Dump atlas to PGM file");
		if (bDumped)
		{
			std::cout << "  Debug atlas dumped to " << DumpPath << "\n";
		}

		const FGlyphInfo* MissingGlyph = FontAsset.FindGlyph(static_cast<uint32>('Z'));
		ReportCheck(Context, MissingGlyph == nullptr, "FindGlyph returns null for uncached glyph");
	}
}

void RunTextModuleConsoleTests(const char* SampleText)
{
	if (!AllocConsole())
	{
		return;
	}

	FILE* OutFile = nullptr;
	FILE* ErrFile = nullptr;
	freopen_s(&OutFile, "CONOUT$", "w", stdout);
	freopen_s(&ErrFile, "CONOUT$", "w", stderr);

	FTestContext Context;
	RunGlyphAtlasTests(Context, SampleText != nullptr ? FString(SampleText) : FString(DEFAULT_SAMPLE_TEXT));

	std::cout << "\n[Summary] Passed=" << Context.Passed << ", Failed=" << Context.Failed << "\n\n";
}

void RunTextModuleConsoleTests()
{
	RunTextModuleConsoleTests(DEFAULT_SAMPLE_TEXT);
}
