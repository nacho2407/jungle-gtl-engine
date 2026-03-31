#pragma once
#include "Asset/AssetManager.h"

struct FAssetLoadParams;

class ENGINE_API IAssetLoader
{
public:
    virtual ~IAssetLoader() = default;

    virtual bool       CanLoad(const FWString& Path, const FAssetLoadParams& Params) const = 0;
    virtual EAssetType GetAssetType() const = 0;

    virtual uint64  MakeBuildSignature(const FAssetLoadParams& Params) const = 0;
    virtual UAsset* LoadAsset(const FSourceRecord& Source, const FAssetLoadParams& Params) = 0;
};