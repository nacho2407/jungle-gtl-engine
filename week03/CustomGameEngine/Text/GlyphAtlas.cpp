#include "Text/GlyphAtlas.h"

#include "EngineCore/Logger.h"

bool FGlyphAtlas::Initialize(uint32 Width, uint32 Height)
{
	if (Width == 0 || Height == 0)
	{
		UE_LOG("[Text/GlyphAtlas][Error] Invalide atlas size");
		return false;
	}

	AltasWidth = Width, AtlasHeight = Height;

	Pixels.clear();
	Pixels.resize(static_cast<size_t>(Width * Height));

	CursorX = 0, CursorY = 0;
	CurrentRowHeight = 0;

	return true;
}

bool FGlyphAtlas::AddGlyphBitmap(const FGlyphBitmap& Bitmap, FAtlasRect& OutRect)
{
	// 작성중
}
