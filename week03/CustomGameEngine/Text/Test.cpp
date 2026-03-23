#include "Text/Test.h"

#include "EngineCore/ResourceManager.h"
#include "Text/FontAsset.h"
#include "Text/TextLayout.h"

#include <Windows.h>
#include <cmath>
#include <cstdio>
#include <iostream>

namespace
{
	const char* DEFAULT_SAMPLE_TEXT = "12-3";
	const float FLOAT_EPSILON = 0.001f;

	struct FTestContext
	{
		int Passed = 0;
		int Failed = 0;
	};

	void EnsureConsoleAttached()
	{
		if (GetConsoleWindow() == nullptr)
		{
			if (!AllocConsole())
			{
				return;
			}
		}

		FILE* OutFile = nullptr;
		FILE* ErrFile = nullptr;
		freopen_s(&OutFile, "CONOUT$", "w", stdout);
		freopen_s(&ErrFile, "CONOUT$", "w", stderr);
	}

	bool NearlyEqual(float A, float B, float Epsilon = FLOAT_EPSILON)
	{
		return std::fabs(A - B) <= Epsilon;
	}

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

	void RunInvalidParamTests(FTestContext& Context)
	{
		std::cout << "[Invalid Params]\n";

		TArray<FVertexFont> Vertices;
		Vertices.push_back({ 1, 2, 3, 4, 5, 6, 7 });

		FTextLayoutBuildParams InvalidParams;
		InvalidParams.FontAsset = nullptr;
		InvalidParams.Text = DEFAULT_SAMPLE_TEXT;
		InvalidParams.Scale = 1.0f;

		const bool bBuilt = BuildBillboardTextVertices(InvalidParams, Vertices);
		ReportCheck(Context, !bBuilt, "Null font asset returns false");
		ReportCheck(Context, Vertices.size() == 1, "Output remains untouched on invalid param failure");
	}

	void RunEmptyStringTests(FTestContext& Context, FFontAsset& FontAsset)
	{
		std::cout << "[Empty String]\n";

		TArray<FVertexFont> Vertices;
		Vertices.push_back({ 1, 2, 3, 4, 5, 6, 7 });

		FTextLayoutBuildParams Params;
		Params.FontAsset = &FontAsset;
		Params.Text = "";
		Params.Anchor = FVector(10.0f, 20.0f, 30.0f);
		Params.Scale = 1.0f;
		Params.LineGap = 4.0f;

		const bool bBuilt = BuildBillboardTextVertices(Params, Vertices);
		ReportCheck(Context, bBuilt, "Empty string returns success");
		ReportCheck(Context, Vertices.empty(), "Empty string emits zero vertices");
	}

	void RunSingleLineLayoutTests(FTestContext& Context, FFontAsset& FontAsset, const FString& SampleText)
	{
		std::cout << "[Single Line Layout]\n";

		const FVector Anchor(10.0f, 20.0f, 30.0f);

		FTextLayoutBuildParams Params;
		Params.FontAsset = &FontAsset;
		Params.Text = SampleText;
		Params.Anchor = Anchor;
		Params.Scale = 1.0f;
		Params.LineGap = 8.0f;

		TArray<FVertexFont> Vertices;
		const bool bBuilt = BuildBillboardTextVertices(Params, Vertices);

		ReportCheck(Context, bBuilt, "BuildBillboardTextVertices succeeds for sample text");
		ReportCheck(Context, Vertices.size() == 24, "Sample text emits 24 vertices for 4 glyphs");

		if (!bBuilt || Vertices.size() != 24)
		{
			return;
		}

		bool bAnchorsValid = true;
		bool bUvsValid = true;

		for (const FVertexFont& Vertex : Vertices)
		{
			if (!NearlyEqual(Vertex.anchorX, Anchor.x) ||
				!NearlyEqual(Vertex.anchorY, Anchor.y) ||
				!NearlyEqual(Vertex.anchorZ, Anchor.z))
			{
				bAnchorsValid = false;
			}

			if (!IsUvInRange(Vertex.u) || !IsUvInRange(Vertex.v))
			{
				bUvsValid = false;
			}
		}

		ReportCheck(Context, bAnchorsValid, "All vertices preserve the requested anchor");
		ReportCheck(Context, bUvsValid, "All generated UVs are normalized");

		bool bGlyphsProgressRight = true;
		float PreviousGlyphMinX = 0.0f;
		bool bHasPreviousGlyph = false;

		for (size_t GlyphIndex = 0; GlyphIndex < 4; ++GlyphIndex)
		{
			const size_t BaseIndex = GlyphIndex * 6;
			float GlyphMinX = Vertices[BaseIndex].offsetX;

			for (size_t VertexIndex = 1; VertexIndex < 6; ++VertexIndex)
			{
				const float OffsetX = Vertices[BaseIndex + VertexIndex].offsetX;
				if (OffsetX < GlyphMinX)
				{
					GlyphMinX = OffsetX;
				}
			}

			if (bHasPreviousGlyph && !(GlyphMinX > PreviousGlyphMinX))
			{
				bGlyphsProgressRight = false;
			}

			PreviousGlyphMinX = GlyphMinX;
			bHasPreviousGlyph = true;
		}

		ReportCheck(Context, bGlyphsProgressRight, "Glyph quads progress from left to right");
	}

	void RunMultilineLayoutTests(FTestContext& Context, FFontAsset& FontAsset)
	{
		std::cout << "[Multiline Layout]\n";

		FTextLayoutBuildParams Params;
		Params.FontAsset = &FontAsset;
		Params.Text = "1\n1";
		Params.Anchor = FVector(0.0f, 0.0f, 0.0f);
		Params.Scale = 1.0f;
		Params.LineGap = 5.0f;

		TArray<FVertexFont> Vertices;
		const bool bBuilt = BuildBillboardTextVertices(Params, Vertices);

		ReportCheck(Context, bBuilt, "Multiline text build succeeds");
		ReportCheck(Context, Vertices.size() == 12, "Two rendered glyphs emit 12 vertices");

		if (!bBuilt || Vertices.size() != 12)
		{
			return;
		}

		float FirstGlyphMaxY = Vertices[0].offsetY;
		for (size_t i = 1; i < 6; ++i)
		{
			if (Vertices[i].offsetY > FirstGlyphMaxY)
			{
				FirstGlyphMaxY = Vertices[i].offsetY;
			}
		}

		float SecondGlyphMaxY = Vertices[6].offsetY;
		float SecondGlyphMinX = Vertices[6].offsetX;
		for (size_t i = 7; i < 12; ++i)
		{
			if (Vertices[i].offsetY > SecondGlyphMaxY)
			{
				SecondGlyphMaxY = Vertices[i].offsetY;
			}
			if (Vertices[i].offsetX < SecondGlyphMinX)
			{
				SecondGlyphMinX = Vertices[i].offsetX;
			}
		}

		ReportCheck(Context, SecondGlyphMaxY < FirstGlyphMaxY, "Newline places the next glyph on a lower line");
		ReportCheck(Context, NearlyEqual(SecondGlyphMinX, Vertices[0].offsetX), "Newline resets horizontal pen position");
	}

	void RunResourceManagerAtlasTests(FTestContext& Context)
	{
		std::cout << "[ResourceManager Atlas]\n";

		ResourceManager* RM = ResourceManager::GetInstance();
		ReportCheck(Context, RM != nullptr, "ResourceManager singleton is available");
		if (RM == nullptr)
		{
			return;
		}

		FMaterial& FontMaterial = RM->GetMaterial(L"Shader/ShaderFont.hlsl");
		ReportCheck(Context, FontMaterial.ShaderResourceView != nullptr, "Font material has atlas SRV after initialization");
		ReportCheck(Context, FontMaterial.SamplerState != nullptr, "Font material has sampler after initialization");
		ReportCheck(Context, FontMaterial.bUseTexture, "Font material keeps texture usage enabled");

		FFontAsset& Font = RM->GetBillboardFont();
		ReportCheck(Context, Font.HasGlyph(static_cast<uint32>('0')), "Prewarmed glyph '0' exists");
		ReportCheck(Context, Font.HasGlyph(static_cast<uint32>('-')), "Prewarmed glyph '-' exists");

		const bool bHadAInitially = Font.HasGlyph(static_cast<uint32>('A'));
		const bool bEnsuredA = RM->EnsureBillboardGlyphs("A");
		ReportCheck(Context, bEnsuredA, "EnsureBillboardGlyphs succeeds for new glyph 'A'");
		ReportCheck(Context, Font.HasGlyph(static_cast<uint32>('A')), "Glyph 'A' exists after ensure");
		ReportCheck(Context, !bHadAInitially, "Glyph 'A' was not prewarmed before ensure");
		ReportCheck(Context, FontMaterial.ShaderResourceView != nullptr, "Font atlas SRV remains valid after glyph update");

		const bool bEnsuredWhitespace = RM->EnsureBillboardGlyphs(" \n");
		ReportCheck(Context, bEnsuredWhitespace, "EnsureBillboardGlyphs succeeds for whitespace and newline");
		ReportCheck(Context, Font.HasGlyph(static_cast<uint32>(' ')), "Whitespace glyph is cached after ensure");

		const bool bEnsuredComposite = RM->EnsureBillboardGlyphs("A0-\n A");
		ReportCheck(Context, bEnsuredComposite, "EnsureBillboardGlyphs succeeds for mixed cached text");
		ReportCheck(Context, FontMaterial.ShaderResourceView != nullptr, "Font atlas SRV stays valid after repeated ensure");
	}

	void PrintExpectedResultsGuide()
	{
		std::cout << "\n[Expected]\n";
		std::cout << "TextLayout sample \"12-3\": success, 24 vertices, UV normalized, glyphs progress right.\n";
		std::cout << "Multiline sample \"1\\n1\": success, 12 vertices, second line lower, X reset.\n";
		std::cout << "ResourceManager atlas: SRV/sampler non-null, prewarmed '0'/'-' present, EnsureBillboardGlyphs(\"A\") succeeds and caches 'A'.\n";
		std::cout << "Overall summary should end with Failed=0.\n";
	}
}

void RunTextLayoutConsoleTests(const char* SampleText)
{
	EnsureConsoleAttached();

	std::cout << "[TextLayout Tests]\n";

	FFontAsset FontAsset;
	const bool bLoaded = FontAsset.LoadFromFile("Assets/Fonts/D2CodingSubset.ttf", 32.0f);

	FTestContext Context;
	ReportCheck(Context, bLoaded, "Load D2CodingSubset font");

	RunInvalidParamTests(Context);

	if (bLoaded)
	{
		RunEmptyStringTests(Context, FontAsset);
		RunSingleLineLayoutTests(Context, FontAsset, SampleText != nullptr ? FString(SampleText) : FString(DEFAULT_SAMPLE_TEXT));
		RunMultilineLayoutTests(Context, FontAsset);
	}

	std::cout << "\n[Summary] Passed=" << Context.Passed << ", Failed=" << Context.Failed << "\n";
}

void RunTextLayoutConsoleTests()
{
	RunTextLayoutConsoleTests(DEFAULT_SAMPLE_TEXT);
}

void RunTextSystemConsoleTests()
{
	EnsureConsoleAttached();

	std::cout << "==============================\n";
	std::cout << "[Text System Tests]\n";

	FTestContext Context;

	FFontAsset FontAsset;
	const bool bLoaded = FontAsset.LoadFromFile("Assets/Fonts/D2CodingSubset.ttf", 32.0f);
	ReportCheck(Context, bLoaded, "Load D2CodingSubset font");

	RunInvalidParamTests(Context);

	if (bLoaded)
	{
		RunEmptyStringTests(Context, FontAsset);
		RunSingleLineLayoutTests(Context, FontAsset, FString(DEFAULT_SAMPLE_TEXT));
		RunMultilineLayoutTests(Context, FontAsset);
	}

	RunResourceManagerAtlasTests(Context);

	std::cout << "\n[Summary] Passed=" << Context.Passed << ", Failed=" << Context.Failed << "\n";
	PrintExpectedResultsGuide();
	std::cout << "==============================\n\n";
}
