#pragma once
#include "AssetLoader.h"
#include "AssetManager.h"
#include "Renderer/RenderAsset/FontResource.h"
#include "ThirdParty/Json/json.hpp"

class FD3D11DynamicRHI;

class FFontAtlasLoader : public IAssetLoader
{
public:
    explicit FFontAtlasLoader(FD3D11DynamicRHI* InRHI);

    bool       CanLoad(const FWString& Path, const FAssetLoadParams& Params) const override;
    EAssetType GetAssetType() const override;
    uint64     MakeBuildSignature(const FAssetLoadParams& Params) const override;
    UAsset*    LoadAsset(const FSourceRecord& Source, const FAssetLoadParams& Params) override;

private:
    bool ParseFontAtlasJson(const FSourceRecord& Source, FFontResource& OutFont) const;

    bool ParseInfo(const nlohmann::json& Root, FFontInfo& OutInfo) const;
    bool ParseCommon(const nlohmann::json& Root, FFontCommon& OutCommon) const;
    bool ParsePages(const nlohmann::json& Root, TArray<FString>& OutPages) const;
    bool ParseChars(const nlohmann::json& Root, TArray<FFontGlyph>& OutGlyphs) const;

    bool LoadAtlasTexture(const FSourceRecord& JsonSource, const FString& PageFile,
                          FTextureResource&    OutAtlas) const;
    FWString ResolveSiblingPath(const FWString& BaseFilePath, const FString& RelativePath) const;

private:
    FD3D11DynamicRHI* RHI = nullptr;
};