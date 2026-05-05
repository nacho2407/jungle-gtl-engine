#include "Texture/Texture2D.h"
#include "Object/ObjectFactory.h"
#include "Core/Log.h"
#include "Platform/Paths.h"
#include "WICTextureLoader.h"

#include <d3d11.h>
#include <filesystem>

IMPLEMENT_CLASS(UTexture2D, UObject)

std::map<FString, UTexture2D*> UTexture2D::TextureCache;

FString UTexture2D::MakeCacheKey(const FString& FilePath, ETextureColorSpace ColorSpace)
{
	return FilePath + (ColorSpace == ETextureColorSpace::SRGB ? "#srgb" : "#linear");
}

UTexture2D::~UTexture2D()
{
	if (SRV)
	{
		if (TrackedTextureMemory > 0)
		{
			MemoryStats::SubTextureMemory(TrackedTextureMemory);
			TrackedTextureMemory = 0;
		}

		SRV->Release();
		SRV = nullptr;
	}

	// 캐시에서 제거
	auto It = TextureCache.find(MakeCacheKey(SourceFilePath, ColorSpace));
	if (It != TextureCache.end() && It->second == this)
	{
		TextureCache.erase(It);
	}
}

void UTexture2D::ReleaseAllGPU()
{
	for (auto& [Path, Texture] : TextureCache)
	{
		if (Texture && Texture->SRV)
		{
			if (Texture->TrackedTextureMemory > 0)
			{
				MemoryStats::SubTextureMemory(Texture->TrackedTextureMemory);
				Texture->TrackedTextureMemory = 0;
			}
			Texture->SRV->Release();
			Texture->SRV = nullptr;
		}
	}
	TextureCache.clear();
}

UTexture2D* UTexture2D::LoadFromFile(const FString& FilePath, ID3D11Device* Device, ETextureColorSpace InColorSpace)
{
	if (FilePath.empty() || !Device) return nullptr;

	// 캐시 히트
	const FString CacheKey = MakeCacheKey(FilePath, InColorSpace);
	auto It = TextureCache.find(CacheKey);
	if (It != TextureCache.end())
	{
		return It->second;
	}

	// 새 UTexture2D 생성
	UTexture2D* Texture = UObjectManager::Get().CreateObject<UTexture2D>();
	if (!Texture->LoadInternal(FilePath, Device, InColorSpace))
	{
		UObjectManager::Get().DestroyObject(Texture);
		return nullptr;
	}

	TextureCache[CacheKey] = Texture;
	return Texture;
}

UTexture2D* UTexture2D::LoadFromCached(const FString& FilePath, ETextureColorSpace InColorSpace)
{
	if (FilePath.empty()) return nullptr;

	auto It = TextureCache.find(MakeCacheKey(FilePath, InColorSpace));
	if (It != TextureCache.end())
	{
		return It->second;
	}

	return nullptr;
}

bool UTexture2D::LoadInternal(const FString& FilePath, ID3D11Device* Device, ETextureColorSpace InColorSpace)
{
	//std::filesystem::path TexPath(FilePath);
	//std::wstring WidePath = TexPath.wstring();
	std::wstring WidePath = FPaths::ToWide(FilePath);

	const auto LoadFlags = (InColorSpace == ETextureColorSpace::SRGB)
		? DirectX::WIC_LOADER_FORCE_SRGB
		: DirectX::WIC_LOADER_IGNORE_SRGB;

	ID3D11Resource* Resource = nullptr;
	HRESULT hr = DirectX::CreateWICTextureFromFileEx(
		Device, WidePath.c_str(),
		0,                                    // maxsize
		D3D11_USAGE_DEFAULT,                  // usage
		D3D11_BIND_SHADER_RESOURCE,           // bindFlags
		0,                                    // cpuAccessFlags
		0,                                    // miscFlags
		LoadFlags,
		&Resource, &SRV);

	if (FAILED(hr))
	{
		UE_LOG("Failed to load texture: %s", FilePath.c_str());
		return false;
	}

	// 텍스처 크기 추출
	if (Resource)
	{
		TrackedTextureMemory = MemoryStats::CalculateTextureMemory(Resource);

		ID3D11Texture2D* Tex2D = nullptr;
		if (SUCCEEDED(Resource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&Tex2D)))
		{
			D3D11_TEXTURE2D_DESC Desc;
			Tex2D->GetDesc(&Desc);
			Width = Desc.Width;
			Height = Desc.Height;
			Tex2D->Release();
		}

		if (TrackedTextureMemory > 0)
		{
			MemoryStats::AddTextureMemory(TrackedTextureMemory);
		}
		Resource->Release();
	}

	SourceFilePath = FilePath;
	ColorSpace = InColorSpace;
	return true;
}
