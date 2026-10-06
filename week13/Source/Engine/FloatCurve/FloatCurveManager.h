#pragma once
#include "Core/Types/CoreTypes.h"
#include "Core/Singleton.h"
#include "Object/GarbageCollection.h"

class UFloatCurveAsset;

class FFloatCurveManager : public TSingleton<FFloatCurveManager>
, public FGCObject
{
	friend class TSingleton<FFloatCurveManager>;

public:
	UFloatCurveAsset* Load(const FString& Path);
	UFloatCurveAsset* Find(const FString& Path) const;
	
	bool Save(UFloatCurveAsset* Asset);


	const char* GetReferencerName() const override { return "FFloatCurveManager"; }
	void AddReferencedObjects(FReferenceCollector& Collector) override;
	void ClearCache();
private:
	TMap<FString, UFloatCurveAsset*> LoadedCurves;
};
