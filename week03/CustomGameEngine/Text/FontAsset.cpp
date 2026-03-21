#include "Text/FontAsset.h"

const FGlyphInfo& FFontAsset::FindGlyph(uint32 Codepoint) const
{
	auto iter = GlyphMap.find(Codepoint);
	if (iter == GlyphMap.end())
	{
		//
	}

	return iter->second;
}

bool FFontAsset::HasGlyph(uint32 Codepoint) const
{
	return GlyphMap.find(Codepoint) != GlyphMap.end();
}
