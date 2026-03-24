#pragma once
#pragma once

#include <memory>

#include "AssetLoader.h"
#include "AssetManager.h"
#include "Renderer/RenderAsset/FontResource.h"
#include "ThirdParty/Json/json.hpp"

class FD3D11RHI;

class ENGINE_API FFontAtlasLoader : public IAssetLoader
{
  public:
    explicit FFontAtlasLoader(FD3D11RHI* InRHI);

    bool       CanLoad(const FWString& Path, const FAssetLoadParams& Params) const override;
    EAssetType GetAssetType() const override;
    uint64     MakeBuildSignature(const FAssetLoadParams& Params) const override;
    UAsset*    LoadAsset(const FSourceRecord& Source, const FAssetLoadParams& Params) override;

  private:
    struct FDecodedAtlasImage
    {
        uint32        Width = 0;
        uint32        Height = 0;
        TArray<uint8> Pixels;
    };

    bool ParseFontAtlasJson(const FSourceRecord& Source, FFontResource& OutFont) const;

    bool ParseInfo(const nlohmann::json& Root, FFontInfo& OutInfo) const;
    bool ParseCommon(const nlohmann::json& Root, FFontCommon& OutCommon) const;
    bool ParsePages(const nlohmann::json& Root, TArray<FString>& OutPages) const;
    bool ParseChars(const nlohmann::json& Root, TMap<uint32, FFontGlyph>& OutGlyphs) const;

    std::shared_ptr<FDecodedAtlasImage> GetOrDecodeAtlas(const FWString& AtlasPath) const;
    std::shared_ptr<FTextureResource>
    GetOrCreateAtlasResource(const FSourceRecord&      AtlasSource,
                             const FDecodedAtlasImage& DecodedImage) const;

    bool DecodeWithWIC(const FSourceRecord& AtlasSource, FDecodedAtlasImage& OutImage) const;
    bool CreateTextureResource(const FDecodedAtlasImage& DecodedImage,
                               FTextureResource&         OutAtlas) const;

    bool     LoadAtlasTexture(const FSourceRecord& JsonSource, const FString& PageFile,
                              FTextureResource& OutAtlas) const;
    FWString ResolveSiblingPath(const FWString& BaseFilePath, const FString& RelativePath) const;

  private:
    FD3D11RHI* RHI = nullptr;

    mutable FSourceCache                                       AtlasSourceCache;
    mutable TMap<FString, std::shared_ptr<FDecodedAtlasImage>> DecodeCache;
    mutable TMap<FString, std::shared_ptr<FTextureResource>>   ResourceCache;
};