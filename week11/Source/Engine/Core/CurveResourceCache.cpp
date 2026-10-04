#include "Core/CurveResourceCache.h"

#include "Core/Paths.h"
#include "Object/ObjectFactory.h"

#include <unordered_set>

UCurveFloatAsset* FCurveResourceCache::Load(const FString& Path)
{
	const FString NormalizedPath = FPaths::Normalize(Path);
	if (NormalizedPath.empty())
	{
		return nullptr;
	}

	auto It = Curves.find(NormalizedPath);
	if (It != Curves.end())
	{
		return It->second;
	}

	UCurveFloatAsset* Curve = CurveLoader.Load(NormalizedPath);
	if (!Curve)
	{
		return nullptr;
	}

	Curves[NormalizedPath] = Curve;
	return Curve;
}

UCurveFloatAsset* FCurveResourceCache::Find(const FString& Path) const
{
	const FString NormalizedPath = FPaths::Normalize(Path);
	auto It = Curves.find(NormalizedPath);
	return It != Curves.end() ? It->second : nullptr;
}

bool FCurveResourceCache::Save(const FString& Path, const UCurveFloatAsset* Curve)
{
	const FString NormalizedPath = FPaths::Normalize(Path);
	if (!CurveLoader.Save(NormalizedPath, Curve))
	{
		return false;
	}

	Curves[NormalizedPath] = const_cast<UCurveFloatAsset*>(Curve);
	return true;
}

void FCurveResourceCache::Release()
{
	std::unordered_set<UObject*> DestroyedObjects;
	auto DestroyUniqueObject = [&DestroyedObjects](UObject* Object)
	{
		if (Object && DestroyedObjects.insert(Object).second)
		{
			UObjectManager::Get().DestroyObject(Object);
		}
	};

	for (auto& [Path, Curve] : Curves)
	{
		DestroyUniqueObject(Curve);
	}
	Curves.clear();
}
