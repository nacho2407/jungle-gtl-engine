#pragma once

#include "Asset/IAssetLoader.h"

class UCurveFloatAsset;
class UCurveVectorAsset;
class UCurveColorAsset;

class FCurveAssetLoader : public IAssetLoader
{
public:
	UCurveFloatAsset* Load(const FString& Path) const;
	UCurveFloatAsset* LoadFloat(const FString& Path) const;
	UCurveVectorAsset* LoadVector(const FString& Path) const;
	UCurveColorAsset* LoadColor(const FString& Path) const;

	bool Save(const FString& Path, const UCurveFloatAsset* Curve) const;
	bool Save(const FString& Path, const UCurveVectorAsset* Curve) const;
	bool Save(const FString& Path, const UCurveColorAsset* Curve) const;

	bool SupportsExtension(const FString& Extension) const override;
};
