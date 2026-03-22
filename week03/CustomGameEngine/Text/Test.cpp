#include "Text/Test.h"

#include "Text/FontAsset.h"
#include "Text/TextEncoding.h"

#include <Windows.h>
#include <cstdio>
#include <iostream>

void RunTextModuleConsoleTests()
{
	if (!AllocConsole())
	{
		return;
	}

	FILE* outFile = nullptr;
	FILE* errFile = nullptr;
	freopen_s(&outFile, "CONOUT$", "w", stdout);
	freopen_s(&errFile, "CONOUT$", "w", stderr);

	const FString fontPath = "Assets/Fonts/D2Coding.ttf";
	const FString sample = "A가한🙂";

	std::cout << "[TextEncoding]\n";
	const FU32String codepoints = Utf8ToCodepoints(sample);
	std::cout << "Input: " << sample << "\n";
	std::cout << "Codepoints(" << codepoints.size() << "): ";
	for (char32_t codepoint : codepoints)
	{
		std::cout << "U+"
			<< std::uppercase
			<< std::hex
			<< static_cast<uint32_t>(codepoint)
			<< ' ';
	}
	std::cout << std::dec << "\n\n";

	std::cout << "[FontAsset]\n";
	FFontAsset fontAsset;
	const bool bLoaded = fontAsset.LoadFromFile(fontPath, 32.0f);
	std::cout << "LoadFromFile(\"" << fontPath << "\") => " << (bLoaded ? "success" : "failed") << "\n";
	if (!bLoaded)
	{
		std::cout << "\n";
		return;
	}

	for (char32_t codepoint : codepoints)
	{
		FGlyphInfo* glyph = fontAsset.EnsureGlyph(static_cast<uint32>(codepoint));
		const FGlyphInfo* foundGlyph = fontAsset.FindGlyph(static_cast<uint32>(codepoint));

		std::cout << "EnsureGlyph(U+"
			<< std::uppercase
			<< std::hex
			<< static_cast<uint32_t>(codepoint)
			<< std::dec
			<< ") => "
			<< (foundGlyph != nullptr ? "cached" : "missing")
			<< ", AdvanceX=" << glyph->Metrics.AdvanceX
			<< ", Bearing=(" << glyph->Metrics.BearingX << ", " << glyph->Metrics.BearingY << ")"
			<< ", Size=(" << glyph->Metrics.Width << ", " << glyph->Metrics.Height << ")\n";

		FGlyphBitmap bitmap{};
		const bool bBitmapBuilt = fontAsset.BuildGlyphBitmap(static_cast<uint32>(codepoint), bitmap);
		std::cout << "BuildGlyphBitmap(U+"
			<< std::uppercase
			<< std::hex
			<< static_cast<uint32_t>(codepoint)
			<< std::dec
			<< ") => "
			<< (bBitmapBuilt ? "success" : "failed");

		if (bBitmapBuilt)
		{
			std::cout
				<< ", Size=(" << bitmap.Width << ", " << bitmap.Height << ")"
				<< ", Offset=(" << bitmap.OffsetX << ", " << bitmap.OffsetY << ")"
				<< ", Pixels=" << bitmap.Pixels.size();
		}

		std::cout << "\n";
	}

	std::cout << "\n";
}
