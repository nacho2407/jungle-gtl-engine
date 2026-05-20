#pragma once

#include "Core/CoreTypes.h"
#include "Core/Singleton.h"
#include "Asset/AssetRegistry.h"

class UAnimGraphAsset;

// UAnimGraphAsset 의 디스크 IO + 캐시. FCameraShakeManager / FFloatCurveManager 와 동일 패턴.
// 같은 NormalizedPath 로 두 번 Load 하면 동일 인스턴스 반환 (캐시 hit).
class FAnimGraphManager : public TSingleton<FAnimGraphManager>
{
	friend class TSingleton<FAnimGraphManager>;

public:
	UAnimGraphAsset* Load(const FString& Path);
	UAnimGraphAsset* Find(const FString& Path) const;

	// Asset->GetSourcePath() 가 비어있으면 false. Save 후에도 LoadedGraphs 캐시 보존.
	bool Save(UAnimGraphAsset* Asset);

	// Content/ 하위 스캔해 디스크의 .uasset 중 EAssetPackageType::AnimGraph 만 목록화.
	// PropertyWidget 의 GraphAsset 콤보가 ListByTypeName("UAnimGraphAsset") 경유로 호출.
	void RefreshAvailableGraphs();
	const TArray<FAssetListItem>& GetAvailableGraphFiles() const { return AvailableGraphFiles; }

private:
	TMap<FString, UAnimGraphAsset*> LoadedGraphs;
	TArray<FAssetListItem>          AvailableGraphFiles;
};
