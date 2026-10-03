#pragma once

#include "Asset/IAssetLoader.h"

class UCurveFloatAsset;

class FCurveAssetLoader : public IAssetLoader
{
public:
    UCurveFloatAsset* Load(const FString& Path) const;
    bool Save(const FString& Path, const UCurveFloatAsset* Curve) const;

    bool SupportsExtension(const FString& Extension) const override;
    FString GetLoaderName() const override;
};
