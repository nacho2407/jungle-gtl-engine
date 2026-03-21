#include "Core/CoreMinimal.h"
#include "Texture2DAsset.h"

#include "AssetManager.h"
#include "Renderer/RenderAsset/FTextureResource.h"

REGISTER_CLASS(, UTexture2DAsset)

void UTexture2DAsset::Initialize(const FSourceRecord& Source, const FTextureBuildSettings& Settings,
                                 std::shared_ptr<FTextureResource> InResource, uint32 InWidth, uint32 InHeight)
{
	InitializeAssetMetadata(Source);

	Width = InWidth;
	Height = InHeight;
	Format = InResource ? InResource->Format : Settings.Format;
	bSRGB = Settings.bSRGB;

	Resource = std::move(InResource);
}

ID3D11ShaderResourceView* UTexture2DAsset::GetSRV() const
{
	return Resource ? Resource->GetSRV() : nullptr;
}
