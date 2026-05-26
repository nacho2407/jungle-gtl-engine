#include "Core/CurveResourceCache.h"

#include "Asset/CurveColorAsset.h"
#include "Asset/CurveVectorAsset.h"
#include "Core/Paths.h"

#include <unordered_set>

UCurveFloatAsset* FCurveResourceCache::Load(const FString& Path)
{
	return LoadFloat(Path);
}

UCurveFloatAsset* FCurveResourceCache::LoadFloat(const FString& Path)
{
	const FString NormalizedPath = FPaths::Normalize(Path);
	if (NormalizedPath.empty())
	{
		return nullptr;
	}

	auto It = FloatCurves.find(NormalizedPath);
	if (It != FloatCurves.end())
	{
		return It->second;
	}

	UCurveFloatAsset* Curve = CurveLoader.LoadFloat(NormalizedPath);
	if (!Curve)
	{
		return nullptr;
	}

	FloatCurves[NormalizedPath] = Curve;
	return Curve;
}

UCurveVectorAsset* FCurveResourceCache::LoadVector(const FString& Path)
{
	const FString NormalizedPath = FPaths::Normalize(Path);
	if (NormalizedPath.empty())
	{
		return nullptr;
	}

	auto It = VectorCurves.find(NormalizedPath);
	if (It != VectorCurves.end())
	{
		return It->second;
	}

	UCurveVectorAsset* Curve = CurveLoader.LoadVector(NormalizedPath);
	if (!Curve)
	{
		return nullptr;
	}

	VectorCurves[NormalizedPath] = Curve;
	return Curve;
}

UCurveColorAsset* FCurveResourceCache::LoadColor(const FString& Path)
{
	const FString NormalizedPath = FPaths::Normalize(Path);
	if (NormalizedPath.empty())
	{
		return nullptr;
	}

	auto It = ColorCurves.find(NormalizedPath);
	if (It != ColorCurves.end())
	{
		return It->second;
	}

	UCurveColorAsset* Curve = CurveLoader.LoadColor(NormalizedPath);
	if (!Curve)
	{
		return nullptr;
	}

	ColorCurves[NormalizedPath] = Curve;
	return Curve;
}

UCurveFloatAsset* FCurveResourceCache::Find(const FString& Path) const
{
	return FindFloat(Path);
}

UCurveFloatAsset* FCurveResourceCache::FindFloat(const FString& Path) const
{
	const FString NormalizedPath = FPaths::Normalize(Path);
	auto It = FloatCurves.find(NormalizedPath);
	return It != FloatCurves.end() ? It->second : nullptr;
}

UCurveVectorAsset* FCurveResourceCache::FindVector(const FString& Path) const
{
	const FString NormalizedPath = FPaths::Normalize(Path);
	auto It = VectorCurves.find(NormalizedPath);
	return It != VectorCurves.end() ? It->second : nullptr;
}

UCurveColorAsset* FCurveResourceCache::FindColor(const FString& Path) const
{
	const FString NormalizedPath = FPaths::Normalize(Path);
	auto It = ColorCurves.find(NormalizedPath);
	return It != ColorCurves.end() ? It->second : nullptr;
}

bool FCurveResourceCache::Save(const FString& Path, const UCurveFloatAsset* Curve)
{
	const FString NormalizedPath = FPaths::Normalize(Path);
	if (!CurveLoader.Save(NormalizedPath, Curve))
	{
		return false;
	}

	FloatCurves[NormalizedPath] = const_cast<UCurveFloatAsset*>(Curve);
	return true;
}

bool FCurveResourceCache::Save(const FString& Path, const UCurveVectorAsset* Curve)
{
	const FString NormalizedPath = FPaths::Normalize(Path);
	if (!CurveLoader.Save(NormalizedPath, Curve))
	{
		return false;
	}

	VectorCurves[NormalizedPath] = const_cast<UCurveVectorAsset*>(Curve);
	return true;
}

bool FCurveResourceCache::Save(const FString& Path, const UCurveColorAsset* Curve)
{
	const FString NormalizedPath = FPaths::Normalize(Path);
	if (!CurveLoader.Save(NormalizedPath, Curve))
	{
		return false;
	}

	ColorCurves[NormalizedPath] = const_cast<UCurveColorAsset*>(Curve);
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

	for (auto& [Path, Curve] : FloatCurves)
	{
		DestroyUniqueObject(Curve);
	}
	FloatCurves.clear();

	for (auto& [Path, Curve] : VectorCurves)
	{
		DestroyUniqueObject(Curve);
	}
	VectorCurves.clear();

	for (auto& [Path, Curve] : ColorCurves)
	{
		DestroyUniqueObject(Curve);
	}
	ColorCurves.clear();
}
