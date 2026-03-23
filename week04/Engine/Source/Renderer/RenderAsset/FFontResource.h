#pragma once

#include "Core/CoreMinimal.h"
#include "Renderer/D3D11/D3D11Common.h"
#include "Resource/TextureResource.h"

#include <dxgiformat.h>

// 글리프 1개 정보
struct FFontGlyph
{
    uint32 Id = 0;

    uint32 X = 0;
    uint32 Y = 0;
    uint32 Width = 0;
    uint32 Height = 0;

    int32 XOffset = 0;
    int32 YOffset = 0;
    int32 XAdvance = 0;

    uint32 Page = 0;
    uint32 Channel = 0;

    bool IsValid() const { return Width > 0 && Height > 0; }
};

struct FFontInfo
{
    FString Face;
    int32   Size = 0;
    bool    bBold = false;
    bool    bItalic = false;
    bool    bUnicode = false;

    int32 StretchH = 100;
    bool  bSmooth = true;
    int32 AA = 1;

    int32 PaddingLeft = 0;
    int32 PaddingTop = 0;
    int32 PaddingRight = 0;
    int32 PaddingBottom = 0;

    int32 SpacingX = 0;
    int32 SpacingY = 0;

    int32 Outline = 0;
};

struct FFontCommon
{
    uint32 LineHeight = 0;
    uint32 Base = 0;

    uint32 ScaleW = 0;
    uint32 ScaleH = 0;

    uint32 Pages = 0;
    bool   bPacked = false;

    uint32 AlphaChannel = 0;
    uint32 RedChannel = 0;
    uint32 GreenChannel = 0;
    uint32 BlueChannel = 0;
};

struct FFontResource
{
    FFontInfo        Info;
    FFontCommon      Common;
    FTextureResource Atlas;

    TArray<FString>          PageFiles;
    TMap<uint32, FFontGlyph> Glyphs;

    const FFontGlyph* FindGlyph(uint32 InCodePoint) const
    {
        auto It = Glyphs.find(InCodePoint);
        return (It != Glyphs.end()) ? &It->second : nullptr;
    }

    const FFontGlyph* FindGlyph(TCHAR InChar) const
    {
        return FindGlyph(static_cast<uint32>(InChar));
    }

    bool HasGlyph(uint32 InCodePoint) const { return Glyphs.find(InCodePoint) != Glyphs.end(); }

    float GetInvAtlasWidth() const
    {
        return Atlas.Width > 0 ? 1.0f / static_cast<float>(Atlas.Width) : 0.0f;
    }

    float GetInvAtlasHeight() const
    {
        return Atlas.Height > 0 ? 1.0f / static_cast<float>(Atlas.Height) : 0.0f;
    }

    ID3D11ShaderResourceView* GetSRV() const { return Atlas.GetSRV(); }

    void Reset()
    {
        Info = {};
        Common = {};
        Atlas = {};
        PageFiles.clear();
        Glyphs.clear();
    }
};