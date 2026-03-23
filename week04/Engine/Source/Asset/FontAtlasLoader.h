#pragma once
#include "AssetLoader.h"
#include "AssetManager.h"
#include "Renderer/RenderAsset/FFontResource.h"


class FD3D11DynamicRHI;

class FFontAtlasLoader : public IAssetLoader
{
public:
    explicit FFontAtlasLoader(FD3D11DynamicRHI* RHI);
    
    bool CanLoad(const FWString& Path, const FAssetLoadParams& Params) const override;
    EAssetType GetAssetType() const override;
    uint64 MakeBuildSignature(const FAssetLoadParams& Params) const override;
    UAsset * LoadAsset(const FSourceRecord& Source, const FAssetLoadParams& Params) override;
    
private:
    bool ParseFontAtlasJson(const FSourceRecord & Source, FFontResource & OutFont) const;
    
    bool ParseInfo(const nlohmann::json)
    
private:
    
};
