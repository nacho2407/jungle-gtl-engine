#pragma once

#include <memory>

#include "AssetLoader.h"
#include "Asset/AssetManager.h"
#include "Renderer/RenderAsset/SubUVAtlasResource.h"
#include "ThirdParty/Json/json.hpp"

class FD3D11RHI;

class ENGINE_API FSubUVAtlasLoader : public IAssetLoader
{
public:
    explicit FSubUVAtlasLoader(FD3D11RHI* InRHI);

    bool       CanLoad(const FWString& Path, const FAssetLoadParams& Params) const override;
    EAssetType GetAssetType() const override;
    UAsset*    LoadAsset(const FSourceRecord& Source, const FAssetLoadParams& Params) override;

private:
    struct FDecodedAtlasImage
    {
        uint32 Width = 0;
        uint32 Height = 0;
        TArray<uint8> Pixels;
    };

    bool ParseSubUVJson(const FSourceRecord& Source, FSubUVAtlasResource& OutAtlas) const;

    bool ParseMeta(const nlohmann::json& Root, FSubUVAtlasResource& OutAtlas) const;
    bool ParseFrames(const nlohmann::json& Root, TArray<FSubUVFrame>& OutFrames) const;
    bool ParseSequences(const TArray<FSubUVFrame>& Frames, TMap<FString, FSubUVSequence>& OutSequences) const;

    std::shared_ptr<FDecodedAtlasImage> GetOrDecodeAtlas(const FWString& AtlasPath) const;
    std::shared_ptr<FTextureResource>   GetOrCreateAtlasResource(const FSourceRecord& AtlasSource,
                                                                 const FDecodedAtlasImage& DecodedImage) const;

    bool DecodeWithWIC(const FSourceRecord& AtlasSource, FDecodedAtlasImage& OutImage) const;
    bool CreateTextureResource(const FDecodedAtlasImage& DecodedImage, FTextureResource& OutAtlas) const;

    bool LoadAtlasTexture(const FSourceRecord& JsonSource, const FString& PageFile, FTextureResource& OutAtlas) const;
    FWString ResolveSiblingPath(const FWString& BaseFilePath, const FString& RelativePath) const;

private:
    FD3D11RHI* RHI = nullptr;

    mutable FSourceCache AtlasSourceCache;
    mutable TMap<FString, std::shared_ptr<FDecodedAtlasImage>> DecodeCache;
    mutable TMap<FString, std::shared_ptr<FTextureResource>> ResourceCache;
};
