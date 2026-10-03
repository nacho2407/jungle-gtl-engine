#pragma once

#include "Asset/CurveAssetLoader.h"
#include "Asset/CurveFloatAsset.h"
#include "Core/CoreTypes.h"

class FCurveResourceCache
{
public:
	UCurveFloatAsset* Load(const FString& Path);
	UCurveFloatAsset* Find(const FString& Path) const;
	bool Save(const FString& Path, const UCurveFloatAsset* Curve);
	void Release();

private:
	FCurveAssetLoader CurveLoader;
	TMap<FString, UCurveFloatAsset*> Curves;
};
