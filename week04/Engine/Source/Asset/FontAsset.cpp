#include "Core/CoreMinimal.h"
#include "FontAsset.h"
#include "AssetManager.h"

void UFontAsset::Initialize(const FSourceRecord& InSource, const FFontResource& InResource)
{
	InitializeAssetMetadata(InSource);
	FontResource = InResource;
}

REGISTER_CLASS(, UFontAsset)