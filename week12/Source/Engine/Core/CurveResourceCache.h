#pragma once

#include "Asset/CurveAssetLoader.h"
#include "Asset/CurveFloatAsset.h"
#include "Core/CoreTypes.h"

class UCurveColorAsset;
class UCurveVectorAsset;

class FCurveResourceCache
{
public:
	UCurveFloatAsset* Load(const FString& Path);
	UCurveFloatAsset* LoadFloat(const FString& Path);
	UCurveVectorAsset* LoadVector(const FString& Path);
	UCurveColorAsset* LoadColor(const FString& Path);

	UCurveFloatAsset* Find(const FString& Path) const;
	UCurveFloatAsset* FindFloat(const FString& Path) const;
	UCurveVectorAsset* FindVector(const FString& Path) const;
	UCurveColorAsset* FindColor(const FString& Path) const;

	bool Save(const FString& Path, const UCurveFloatAsset* Curve);
	bool Save(const FString& Path, const UCurveVectorAsset* Curve);
	bool Save(const FString& Path, const UCurveColorAsset* Curve);
	void Release();

private:
	FCurveAssetLoader CurveLoader;
	TMap<FString, UCurveFloatAsset*> FloatCurves;
	TMap<FString, UCurveVectorAsset*> VectorCurves;
	TMap<FString, UCurveColorAsset*> ColorCurves;
};
