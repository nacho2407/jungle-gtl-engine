#pragma once

#include "Asset.h"
#include "AssetManager.h"
#include "Renderer/RenderAsset/SubUVAtlasResource.h"

class ENGINE_API USubUVAtlasAsset : public UAsset
{
public:
    DECLARE_RTTI(USubUVAtlasAsset, UAsset)

    void Initialize(const FSourceRecord& InSource, const FSubUVAtlasResource& InResource);

    const FSubUVAtlasResource& GetResource() const { return AtlasResource; }
    FSubUVAtlasResource&       GetResource() { return AtlasResource; }

private:
    FSubUVAtlasResource AtlasResource;
};
