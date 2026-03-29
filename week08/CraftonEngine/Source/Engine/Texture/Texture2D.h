#pragma once

#include "Object/Object.h"
#include "Core/CoreTypes.h"

#include <map>
#include <string>

struct ID3D11Device;
struct ID3D11ShaderResourceView;

// UTexture2D — 텍스처 에셋 (SRV를 소유하는 UObject)
// 같은 경로의 텍스처는 캐시를 통해 하나의 UTexture2D를 공유합니다.
class UTexture2D : public UObject
{
public:
	DECLARE_CLASS(UTexture2D, UObject)

	UTexture2D() = default;
	~UTexture2D() override;

	// 경로로 텍스처를 로드 (캐시 히트 시 기존 객체 반환)
	static UTexture2D* LoadFromFile(const FString& FilePath, ID3D11Device* Device);

	ID3D11ShaderResourceView* GetSRV() const { return SRV; }
	const FString& GetSourcePath() const { return SourceFilePath; }
	uint32 GetWidth() const { return Width; }
	uint32 GetHeight() const { return Height; }
	bool IsLoaded() const { return SRV != nullptr; }

private:
	bool LoadInternal(const FString& FilePath, ID3D11Device* Device);

	FString SourceFilePath;
	ID3D11ShaderResourceView* SRV = nullptr;
	uint32 Width = 0;
	uint32 Height = 0;

	// path → UTexture2D* 캐시 (소유권은 UObjectManager)
	static std::map<FString, UTexture2D*> TextureCache;
};
