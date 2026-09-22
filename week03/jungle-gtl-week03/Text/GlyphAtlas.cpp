#include "Text/GlyphAtlas.h"

#include "EngineCore/Logger.h"
#include <fstream>

bool FGlyphAtlas::Initialize(uint32 Width, uint32 Height)
{
	if (Width == 0 || Height == 0)
	{
		UE_LOG("[Text/GlyphAtlas][Error] Invalide atlas size");
		return false;
	}

	AtlasWidth = Width, AtlasHeight = Height;

	Pixels.clear();
	Pixels.resize(static_cast<size_t>(Width * Height));

	CursorX = 0, CursorY = 0;
	CurrentRowHeight = 0;

	return true;
}

bool FGlyphAtlas::AddGlyphBitmap(const FGlyphBitmap& Bitmap, FAtlasRect& OutRect)
{
	if (Bitmap.Width <= 0 || Bitmap.Height <= 0)
	{
		return false;
	}

	if (CursorX + Bitmap.Width > AtlasWidth)
	{
		CursorX = 0;
		CursorY += CurrentRowHeight + ATLAS_PAD;
		CurrentRowHeight = 0;
	}

	if (CursorY + Bitmap.Height > AtlasHeight)
	{
		UE_LOG("[Text/GlyphAtlas][Warning] Atlas is full");
		return false;
	}

	for (uint32 y = 0; y < static_cast<uint32>(Bitmap.Height); y++)
	{
		// 한 줄 단위로 std::memcpy
		uint32 DstOffset = (CursorY + y) * AtlasWidth + CursorX;
		uint32 SrcOffset = y * Bitmap.Width;

		std::memcpy(&Pixels[DstOffset], &Bitmap.Pixels[SrcOffset], static_cast<size_t>(Bitmap.Width));
	}

	OutRect.X = CursorX;
	OutRect.Y = CursorY;
	OutRect.Width = Bitmap.Width;
	OutRect.Height = Bitmap.Height;

	CursorX += Bitmap.Width + ATLAS_PAD;

	if (Bitmap.Height > CurrentRowHeight)
	{
		CurrentRowHeight = Bitmap.Height;
	}

	return true;
}

bool FGlyphAtlas::SaveDebugPGM(const FString& FilePath) const
{
	if (FilePath.empty())
	{
		UE_LOG("[Text/GlyphAtlas][Error] Invalid debug atlas path");
		return false;
	}

	if (AtlasWidth == 0 || AtlasHeight == 0 || Pixels.empty())
	{
		UE_LOG("[Text/GlyphAtlas][Error] Atlas is not initialized");
		return false;
	}

	std::ofstream Output(FilePath, std::ios::binary);
	if (!Output.is_open())
	{
		UE_LOG("[Text/GlyphAtlas][Error] Cannot open debug atlas output file");
		return false;
	}

	Output << "P5\n" << AtlasWidth << ' ' << AtlasHeight << "\n255\n";
	Output.write(reinterpret_cast<const char*>(Pixels.data()), static_cast<std::streamsize>(Pixels.size()));

	if (!Output.good())
	{
		UE_LOG("[Text/GlyphAtlas][Error] Failed to write debug atlas file");
		return false;
	}

	return true;
}
