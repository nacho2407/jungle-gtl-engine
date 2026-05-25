#pragma once
#include "Core/Types/CoreTypes.h"
#include "Core/Singleton.h"
#include "Object/GarbageCollection.h"

class UCameraShakeAsset;

class FCameraShakeManager : public TSingleton<FCameraShakeManager>
, public FGCObject
{
	friend class TSingleton<FCameraShakeManager>;

public:
	UCameraShakeAsset* Load(const FString& Path);
	UCameraShakeAsset* Find(const FString& Path) const;

	bool Save(UCameraShakeAsset* Asset);


	void AddReferencedObjects(FReferenceCollector& Collector) override;
private:
	TMap<FString, UCameraShakeAsset*> LoadedShakes;
};
