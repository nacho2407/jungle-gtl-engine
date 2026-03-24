#pragma once
#include <memory>

#include "AssetLoader.h"
#include "Renderer/D3D11/D3D11DynamicRHI.h"

struct FTextureResource;
struct FDecodedImage;

// Decode cache 값
struct FDecodedImage
{
	uint32 Width = 0;
	uint32 Height = 0;
	DXGI_FORMAT Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	TArray<uint8> Pixels;
};


class ENGINE_API FTextureLoader : public IAssetLoader
{
public:
	explicit FTextureLoader(FD3D11DynamicRHI* InRHI);

	bool CanLoad(const FWString& Path, const FAssetLoadParams& Params) const override;
	EAssetType GetAssetType() const override;
	uint64 MakeBuildSignature(const FAssetLoadParams& Params) const override;
	UAsset* LoadAsset(const FSourceRecord& Source, const FAssetLoadParams& Params) override;

private:
	std::shared_ptr<FDecodedImage> GetOrDecode(const FSourceRecord& Source);
	std::shared_ptr<FTextureResource> GetOrCreateResource(const FString& SourceHash, const FDecodedImage& Image,
	                                                      const FTextureBuildSettings& Settings);
	bool DecodeWithWIC(const FSourceRecord& Source, FDecodedImage& OutImage);
	bool CreateTextureResource(const FDecodedImage& Image, const FTextureBuildSettings& Settings, FTextureResource& OutResource);

private:
	FD3D11DynamicRHI* RHI = nullptr;
	TMap<FString, std::shared_ptr<FDecodedImage>> DecodeCache;
	TMap<FTextureBuildKey, std::shared_ptr<FTextureResource>, FTextureBuildKeyHasher> ResourceCache;

};
