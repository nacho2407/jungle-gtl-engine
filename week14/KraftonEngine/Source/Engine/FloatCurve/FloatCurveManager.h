#pragma once
#include "Core/CoreTypes.h"
#include "Core/Singleton.h"

class UFloatCurveAsset;

class FFloatCurveManager : public TSingleton<FFloatCurveManager>
{
	friend class TSingleton<FFloatCurveManager>;

public:
	UFloatCurveAsset* Load(const FString& Path);
	UFloatCurveAsset* Find(const FString& Path) const;
	
	bool Save(UFloatCurveAsset* Asset);

private:
	TMap<FString, UFloatCurveAsset*> LoadedCurves;
};
