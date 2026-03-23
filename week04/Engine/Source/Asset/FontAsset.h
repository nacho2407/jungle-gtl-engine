#pragma once
#include "Asset.h"
#include "AssetManager.h"
#include "Renderer/RenderAsset/FFontResource.h"

class ENGINE_API UFontAsset : public UAsset
{
public:
    DECLARE_RTTI(UFontAsset, UAsset)

    void Initialize(const FSourceRecord& InSource, const FFontResource& InResource)
    {
        FontResource = InResource;
        SourceRecord = InSource;
    }

    const FFontResource& GetResource() const { return FontResource; }
    FFontResource&       GetResource() { return FontResource; }

private:
    FFontResource FontResource;
    FSourceRecord SourceRecord;
};