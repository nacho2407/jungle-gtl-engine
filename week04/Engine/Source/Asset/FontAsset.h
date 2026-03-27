#pragma once
#include "Asset.h"
#include "AssetManager.h"
#include "Renderer/RenderAsset/FontResource.h"

class ENGINE_API UFontAsset : public UAsset
{
    DECLARE_RTTI(UFontAsset, UAsset)
  public:
    UFontAsset() = default;
    ~UFontAsset() = default;

    void Initialize(const FSourceRecord& InSource, const FFontResource& InResource);

    const FFontResource& GetResource() const { return FontResource; }
    FFontResource&       GetResource() { return FontResource; }

  private:
    FFontResource FontResource;
};