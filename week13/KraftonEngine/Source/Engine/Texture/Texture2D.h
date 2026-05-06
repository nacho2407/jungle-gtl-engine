#pragma once

#include "Object/Object.h"
#include "Core/CoreTypes.h"

#include <map>
#include <string>

struct ID3D11Device;
struct ID3D11ShaderResourceView;

enum class ETextureColorSpace : uint8
{
	Linear,
	SRGB
};

// UTexture2D — 텍스처 에셋 (SRV를 소유하는 UObject)
// 같은 경로의 텍스처는 캐시를 통해 하나의 UTexture2D를 공유합니다.
class UTexture2D : public UObject
{
public:
	DECLARE_CLASS(UTexture2D, UObject)

	UTexture2D() = default;
	~UTexture2D() override;

	// 경로로 텍스처를 로드 (캐시 히트 시 기존 객체 반환)
	static UTexture2D* LoadFromFile(const FString& FilePath, ID3D11Device* Device, ETextureColorSpace ColorSpace = ETextureColorSpace::SRGB);
	static UTexture2D* LoadFromCached(const FString& FilePath, ETextureColorSpace ColorSpace = ETextureColorSpace::SRGB);

	// 캐시된 모든 텍스처의 GPU 리소스 해제 (Shutdown 시 Device 해제 전 호출)
	static void ReleaseAllGPU();

	ID3D11ShaderResourceView* GetSRV() const { return SRV; }
	const FString& GetSourcePath() const { return SourceFilePath; }
	uint32 GetWidth() const { return Width; }
	uint32 GetHeight() const { return Height; }
	bool IsLoaded() const { return SRV != nullptr; }

private:
	static FString MakeCacheKey(const FString& FilePath, ETextureColorSpace ColorSpace);
	bool LoadInternal(const FString& FilePath, ID3D11Device* Device, ETextureColorSpace ColorSpace);

	FString SourceFilePath;
	ID3D11ShaderResourceView* SRV = nullptr;
	uint32 Width = 0;
	uint32 Height = 0;
	uint64 TrackedTextureMemory = 0;
	ETextureColorSpace ColorSpace = ETextureColorSpace::SRGB;

	// path → UTexture2D* 캐시 (소유권은 UObjectManager)
	static std::map<FString, UTexture2D*> TextureCache;
};
