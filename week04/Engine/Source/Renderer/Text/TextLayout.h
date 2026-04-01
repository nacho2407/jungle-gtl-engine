#pragma once

#include "Core/CoreMinimal.h"
#include "Core/Geometry/Primitives/AABB.h"
#include "Renderer/Types/RenderItem.h"

struct FTextLayoutGlyph
{
    const FFontGlyph* Glyph = nullptr;

    float MinX = 0.0f;
    float MinY = 0.0f;
    float MaxX = 0.0f;
    float MaxY = 0.0f;

    bool   bSolidColorQuad = false;
    FColor SolidColor = FColor::White();
};

struct FTextLayoutResult
{
    TArray<FTextLayoutGlyph> Glyphs;

    float MinX = 0.0f;
    float MinY = 0.0f;
    float MaxX = 0.0f;
    float MaxY = 0.0f;

    float GetWidth() const { return MaxX - MinX; }
    float GetHeight() const { return MaxY - MinY; }
    bool  HasGlyphs() const { return !Glyphs.empty(); }
    bool  IsValid() const { return HasGlyphs() && GetWidth() > 0.0f && GetHeight() > 0.0f; }
};

struct FTextWorldQuad
{
    FVector BottomLeft = FVector::ZeroVector;
    FVector Right = FVector::ZeroVector;
    FVector Up = FVector::ZeroVector;
};

struct FTextWorldGeometry
{
    TArray<FTextWorldQuad> GlyphQuads;
    Geometry::FAABB        WorldAABB;
    bool                   bHasWorldAABB = false;

    bool HasQuads() const { return !GlyphQuads.empty(); }
};

ENGINE_API FTextLayoutResult BuildTextLayout(const FTextRenderItem& InItem);
ENGINE_API FTextWorldGeometry BuildTextWorldGeometry(const FTextRenderItem& InItem,
                                                     const FMatrix&         InViewMatrix);
