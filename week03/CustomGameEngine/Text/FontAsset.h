#pragma once

#include "EngineCore/EngineTypes.h"
#include "Text/FontTypes.h"

class FFontAsset
{
public:
	const FGlyphInfo& FindGlyph(uint32 Codepoint) const;
	bool HasGlyph(uint32 Codepoint) const;
private:
	TMap<uint32, FGlyphInfo> GlyphMap;
};
