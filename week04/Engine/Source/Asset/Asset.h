#pragma once
#include "CoreUObject/Object.h"

struct FSourceRecord;

class ENGINE_API UAsset : public UObject
{
public:
	DECLARE_RTTI(UAsset, UObject)
	UAsset() = default;
	~UAsset() override = default;

public:
    const FWString& GetPath() const { return SourcePath; }
    void SetPath(const FWString& InPath) { SourcePath = InPath; }
    const FString& GetHash() const { return ImportedHash; }
    const FString& GetAssetName() const { return AssetName; }

    bool IsDirty() const { return bDirty; }

protected:
    void InitializeAssetMetadata(const FSourceRecord& Source);
    void SetAssetName(const FString& InAssetName) { AssetName = InAssetName; }
    void MarkDirty() { bDirty = true; }
    void ClearDirty() { bDirty = false; }

protected:
    FWString SourcePath;
    FString ImportedHash;
    FString AssetName;
    bool bDirty = false;
};
