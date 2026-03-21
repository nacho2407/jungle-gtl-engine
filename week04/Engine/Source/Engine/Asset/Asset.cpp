#include "Core/CoreMinimal.h"
#include "Asset.h"

#include "AssetManager.h"

REGISTER_CLASS(, UAsset)

void UAsset::InitializeAssetMetadata(const FSourceRecord& Source)
{
	SourcePath = Source.NormalizedPath;
	ImportedHash = Source.SourceHash;
}
