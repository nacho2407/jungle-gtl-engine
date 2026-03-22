#define STB_TRUETYPE_IMPLEMENTATION

#include "Text/FontAsset.h"

#include <cstring>
#include <fstream>

#include "EngineCore/Logger.h"

/**
 * @def DEFAULT_ATLAS_WIDTH Asset이 들고 있는 Atlas의 기본 너비
 */
#define DEFAULT_ATLAS_WIDTH 2048

 /**
  * @def DEFAULT_ATLAS_HEIGHT Asset이 들고 있는 Atlas의 기본 높이
  */
#define DEFAULT_ATLAS_HEIGHT 2048

const FGlyphInfo* FFontAsset::FindGlyph(uint32 Codepoint) const
{
	auto iter = GlyphMap.find(Codepoint);
	if (iter == GlyphMap.end())
	{
		return nullptr;
	}

	return &(iter->second);
}

bool FFontAsset::HasGlyph(uint32 Codepoint) const
{
	return GlyphMap.find(Codepoint) != GlyphMap.end();
}

bool FFontAsset::LoadFromFile(const FString& FontPath, float PixelHeight)
{
	GlyphMap.clear();
	FontData.clear();
	FontPixelHeight = 0.0f;
	bLoaded = false;

	if (FontPath.empty())
	{
		UE_LOG("[Text/FontAsset][Error] Invalid font file path");
		return false;
	}

	if (PixelHeight <= 0.0f)
	{
		UE_LOG("[Text/FontAsset][Error] Invalid pixel height");
		return false;
	}

	std::ifstream FontFile(FontPath, std::ios::binary);
	if (!FontFile.is_open())
	{
		UE_LOG("[Text/FontAsset][Error] Cannot open font file");
		return false;
	}

	FontFile.seekg(0, std::ios::end);
	uint64 FontFileSize = static_cast<uint64>(FontFile.tellg());
	FontFile.seekg(0, std::ios::beg);

	if (FontFileSize == 0)
	{
		UE_LOG("[Text/FontAsset][Error] Invalid font file(size 0)");
		return false;
	}

	FontData.resize(static_cast<std::size_t>(FontFileSize));
	if (!FontFile.read(reinterpret_cast<char*>(FontData.data()), static_cast<std::streamsize>(FontFileSize)))
	{
		UE_LOG("[Text/FontAsset][Error] Cannot read font file");
		FontData.clear();
		return false;
	}

	const int FontOffset = stbtt_GetFontOffsetForIndex(FontData.data(), 0);
	if (FontOffset < 0)
	{
		UE_LOG("[Text/FontAsset][Error] Cannot find font face");
		FontData.clear();
		return false;
	}

	if (!stbtt_InitFont(&FontInfo, FontData.data(), FontOffset))
	{
		UE_LOG("[Text/FontAsset][Error] Cannot initialize font informations");
		FontData.clear();
		return false;
	}

	FontPixelHeight = PixelHeight;
	bLoaded = true;

	Atlas.Initialize(DEFAULT_ATLAS_WIDTH, DEFAULT_ATLAS_HEIGHT);

	return true;
}

FGlyphInfo* FFontAsset::EnsureGlyph(uint32 Codepoint)
{
	auto iter = GlyphMap.find(Codepoint);
	if (iter != GlyphMap.end())
	{
		return &(iter->second);
	}

	FGlyphInfo NewGlyph{};
	NewGlyph.Codepoint = Codepoint;

	if (!bLoaded)
	{
		UE_LOG("[Text/FontAsset][Error] Font is not loaded");
		return nullptr;
	}

	const int GlyphIndex = stbtt_FindGlyphIndex(&FontInfo, static_cast<int>(Codepoint));
	if (GlyphIndex == 0)
	{
		UE_LOG("[Text/FontAsset][Warning] Missing glyph for codepoint");
		return nullptr;
	}

	const float Scale = stbtt_ScaleForPixelHeight(&FontInfo, FontPixelHeight);
	int Advance = 0;
	int LeftSideBearing = 0;
	stbtt_GetCodepointHMetrics(&FontInfo, static_cast<int>(Codepoint), &Advance, &LeftSideBearing);

	int X0 = 0;
	int Y0 = 0;
	int X1 = 0;
	int Y1 = 0;
	stbtt_GetCodepointBitmapBox(&FontInfo, static_cast<int>(Codepoint), Scale, Scale, &X0, &Y0, &X1, &Y1);

	NewGlyph.Metrics.AdvanceX = Advance * Scale;
	NewGlyph.Metrics.BearingX = static_cast<float>(X0);
	NewGlyph.Metrics.BearingY = static_cast<float>(-Y0);
	NewGlyph.Metrics.Width = static_cast<float>(X1 - X0);
	NewGlyph.Metrics.Height = static_cast<float>(Y1 - Y0);

	FGlyphBitmap Bitmap;
	if (!BuildGlyphBitmap(Codepoint, Bitmap))
	{
		UE_LOG("[Text/FontAsset][Error] Failed to build glyph bitmap");
		return nullptr;
	}

	FAtlasRect AtlasRect;
	if (!Atlas.AddGlyphBitmap(Bitmap, AtlasRect))
	{
		UE_LOG("[Text/FontAsset][Error] Faild to add glyph bitmap to atlas");
		return nullptr;
	}

	NewGlyph.AtlasRect = AtlasRect;

	const float AtlasWidthF = static_cast<float>(Atlas.GetWidth());
	const float AtlasHeightF = static_cast<float>(Atlas.GetHeight());

	NewGlyph.U0 = static_cast<float>(AtlasRect.X) / AtlasWidthF;
	NewGlyph.V0 = static_cast<float>(AtlasRect.Y) / AtlasHeightF;
	NewGlyph.U1 = static_cast<float>(AtlasRect.X + AtlasRect.Width) / AtlasWidthF;
	NewGlyph.V1 = static_cast<float>(AtlasRect.Y + AtlasRect.Height) / AtlasHeightF;

	auto Result = GlyphMap.emplace(Codepoint, std::move(NewGlyph));
	return &(Result.first->second);
}

bool FFontAsset::BuildGlyphBitmap(uint32 Codepoint, FGlyphBitmap& OutBitmap) const
{
	OutBitmap = FGlyphBitmap{};

	if (!bLoaded)
	{
		UE_LOG("[Text/FontAsset][Error] The font file has not yet been loaded");
		return false;
	}

	const int32 GlyphIndex = stbtt_FindGlyphIndex(&FontInfo, static_cast<int>(Codepoint));
	if (GlyphIndex == 0)
	{
		UE_LOG("[Text/FontAsset][Warning] Missing glyph for codepoint");
		return false;
	}

	const float FontScale = stbtt_ScaleForPixelHeight(&FontInfo, FontPixelHeight);

	int Width = 0, Height = 0;
	int OffsetX = 0, OffsetY = 0;

	uint8* BitmapBuffer = stbtt_GetCodepointBitmap(
		&FontInfo,
		FontScale,
		FontScale,
		static_cast<int>(Codepoint),
		&Width,
		&Height,
		&OffsetX,
		&OffsetY
	);

	if (BitmapBuffer == nullptr)
	{
		UE_LOG("[Text/FontAsset][Error] Failed to build glyph bitmap");
		return false;
	}

	OutBitmap.Width = Width;
	OutBitmap.Height = Height;
	OutBitmap.OffsetX = OffsetX;
	OutBitmap.OffsetY = OffsetY;

	const int32 PixelCount = static_cast<int32>(Width * Height);
	OutBitmap.Pixels.resize(PixelCount);

	std::memcpy(OutBitmap.Pixels.data(), BitmapBuffer, static_cast<size_t>(PixelCount));

	stbtt_FreeBitmap(BitmapBuffer, nullptr);

	return true;
}
