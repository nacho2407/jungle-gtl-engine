#pragma once
#include "CoreUObject/Object.h"

class ENGINE_API UAsset : public UObject
{
public:
	DECLARE_RTTI(UAsset, UObject)

	UAsset() = default;
	~UAsset() override = default;
	
public:
	FString GetPath() const { return SourcePath; }
	FString GetHash() const { return ImportedHash; }



protected:
	FString SourcePath;
	FString ImportedHash;
	FString AssetName;
	bool bDirty = false;
};
