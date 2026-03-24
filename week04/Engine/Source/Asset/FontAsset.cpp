#include "Core/CoreMinimal.h"
#include "FontAsset.h"

#include "AssetManager.h"

REGISTER_CLASS(, UFontAsset)

void UFontAsset::Initialize(const FSourceRecord& InSource, const FFontResource& InResource)
{
	InitializeAssetMetadata(InSource);
	FontResource = InResource;
}
