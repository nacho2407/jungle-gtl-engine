#pragma once

#include "Core/CoreMinimal.h"
#include "Renderer/RenderAsset/TextureResource.h"

struct FSubUVAtlasInfo
{
    FString Name;
    FString Texture;

    uint32 FrameWidth = 0;
    uint32 FrameHeight = 0;
    uint32 Columns = 1;
    uint32 Rows = 1;
    uint32 FrameCount = 0;

    float FPS = 0.0f;
    bool  bLoop = true;
};

struct FSubUVAtlasCommon
{
    uint32 ScaleW = 0;
    uint32 ScaleH = 0;
    uint32 Pages = 0;
    bool   bPacked = false;
};

struct FSubUVFrame
{
    uint32 Id = 0;
    uint32 X = 0;
    uint32 Y = 0;
    uint32 Width = 0;
    uint32 Height = 0;

    float PivotX = 0.5f;
    float PivotY = 0.5f;

    float Duration = 0.0f;

    bool IsValid() const { return Width > 0 && Height > 0; }
};

struct FSubUVSequence
{
    FString Name;
    uint32  StartFrame = 0;
    uint32  EndFrame = 0;
    bool    bLoop = true;
};

struct FSubUVAtlasResource
{
    FSubUVAtlasInfo   Info;
    FSubUVAtlasCommon Common;
    FTextureResource  Atlas;

    TArray<FString>       PageFiles;
    TArray<FSubUVFrame>   Frames;
    TMap<FString, FSubUVSequence> Sequences;

    const FSubUVFrame* FindFrame(uint32 InId) const
    {
        for (const FSubUVFrame& Frame : Frames)
        {
            if (Frame.Id == InId)
            {
                return &Frame;
            }
        }
        return nullptr;
    }

    const FSubUVSequence* FindSequence(const FString& InName) const
    {
        auto It = Sequences.find(InName);
        return It != Sequences.end() ? &It->second : nullptr;
    }

    ID3D11ShaderResourceView* GetSRV() const { return Atlas.GetSRV(); }

    void Reset()
    {
        Info = {};
        Common = {};
        Atlas = {};
        PageFiles.clear();
        Frames.clear();
        Sequences.clear();
    }
};
