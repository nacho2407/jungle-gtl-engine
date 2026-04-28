#include "Render/Submission/Atlas/ShadowAtlasLayoutUtils.h"

#include <algorithm>

FShadowAtlasRect MakeShadowAtlasViewportRect(const FShadowAtlasRect& Rect)
{
    FShadowAtlasRect Viewport = Rect;
    const uint32 Padding = std::min(ShadowAtlas::Padding, Rect.Width / 2);
    Viewport.X += Padding;
    Viewport.Y += Padding;
    Viewport.Width = (Rect.Width > Padding * 2) ? (Rect.Width - Padding * 2) : Rect.Width;
    Viewport.Height = (Rect.Height > Padding * 2) ? (Rect.Height - Padding * 2) : Rect.Height;
    return Viewport;
}

FVector4 MakeShadowAtlasUVScaleOffset(const FShadowAtlasRect& ViewportRect)
{
    const float AtlasSize = static_cast<float>(ShadowAtlas::AtlasSize);
    return FVector4(
        static_cast<float>(ViewportRect.Width) / AtlasSize,
        static_cast<float>(ViewportRect.Height) / AtlasSize,
        static_cast<float>(ViewportRect.X) / AtlasSize,
        static_cast<float>(ViewportRect.Y) / AtlasSize);
}
