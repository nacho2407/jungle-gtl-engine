#pragma once
#include "CoreUObject/Object.h"

class ENGINE_API UAsset : public UObject
{
public:
	DECLARE_RTTI(UAsset, UObject)

	UAsset() = default;
	~UAsset() override = default;
	


public:
	const WFString& GetPath() const { return SourcePath; }
	const FString& GetHash() const { return ImportedHash; }



protected:
	WFString SourcePath;
	FString ImportedHash;
	FString AssetName;

	bool bDirty = false;
};
