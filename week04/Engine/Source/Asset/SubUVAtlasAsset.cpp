#include "Core/CoreMinimal.h"
#include "SubUVAtlasAsset.h"

#include "AssetManager.h"

REGISTER_CLASS(, USubUVAtlasAsset)

void USubUVAtlasAsset::Initialize(const FSourceRecord& InSource, const FSubUVAtlasResource& InResource)
{
    InitializeAssetMetadata(InSource);
    AtlasResource = InResource;
}
