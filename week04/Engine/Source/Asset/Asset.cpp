#include "Core/CoreMinimal.h"
#include "Asset.h"

#include "AssetManager.h"

void UAsset::InitializeAssetMetadata(const FSourceRecord& Source)
{
    SourcePath = Source.NormalizedPath;
    ImportedHash = Source.SourceHash;
}

REGISTER_CLASS(, UAsset)
