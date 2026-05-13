#pragma once
#include "Core/CoreTypes.h"
#include "Core/Singleton.h"

class UCameraShakeAsset;

class FCameraShakeManager : public TSingleton<FCameraShakeManager>
{
	friend class TSingleton<FCameraShakeManager>;

public:
	UCameraShakeAsset* Load(const FString& Path);
	UCameraShakeAsset* Find(const FString& Path) const;

	bool Save(UCameraShakeAsset* Asset);

private:
	TMap<FString, UCameraShakeAsset*> LoadedShakes;
};
