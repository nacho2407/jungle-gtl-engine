#include "Texture/Texture2D.h"
#include "Object/Reflection/ObjectFactory.h"
#include "Core/Logging/Log.h"
#include "Platform/Paths.h"
#include "WICTextureLoader.h"
#include "stb_image.h"

#include "Object/ReferenceCollector.h"

#include <algorithm>
#include <cwctype>
#include <d3d11.h>
#include <filesystem>
#include <fstream>
#include <vector>

std::map<FString, UTexture2D*> UTexture2D::TextureCache;

void UTexture2D::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (auto& [Path, Texture] : TextureCache)
	{
		Collector.AddReferencedObject(Texture);
	}
}

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

namespace
{
	// .tga 같이 WIC 가 native 로 못 까는 확장자를 구분.
	bool IsStbHandledExtension(const FString& FilePath)
	{
		std::wstring Ext = std::filesystem::path(FPaths::ToWide(FilePath)).extension().wstring();
		std::transform(Ext.begin(), Ext.end(), Ext.begin(),
			[](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
		return Ext == L".tga";
	}
}

bool UTexture2D::LoadInternal(const FString& FilePath, ID3D11Device* Device, ETextureColorSpace InColorSpace)
{
	if (IsStbHandledExtension(FilePath))
	{
		return LoadInternal_STB(FilePath, Device, InColorSpace);
	}

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

bool UTexture2D::LoadInternal_STB(const FString& FilePath, ID3D11Device* Device, ETextureColorSpace InColorSpace)
{
	// stbi_load 는 fopen(UTF-8) 기반이라 한글/공백 경로에서 깨질 수 있어
	// wide ifstream 으로 직접 읽고 stbi_load_from_memory 로 우회.
	const std::wstring WidePath = FPaths::ToWide(FilePath);
	std::ifstream File(WidePath, std::ios::binary | std::ios::ate);
	if (!File.is_open())
	{
		UE_LOG("Failed to open texture file: %s", FilePath.c_str());
		return false;
	}

	const std::streamsize FileSize = File.tellg();
	if (FileSize <= 0)
	{
		UE_LOG("Empty texture file: %s", FilePath.c_str());
		return false;
	}
	File.seekg(0, std::ios::beg);

	std::vector<uint8> FileBytes(static_cast<size_t>(FileSize));
	if (!File.read(reinterpret_cast<char*>(FileBytes.data()), FileSize))
	{
		UE_LOG("Failed to read texture file: %s", FilePath.c_str());
		return false;
	}
	File.close();

	int W = 0;
	int H = 0;
	int OrigChannels = 0;
	stbi_uc* Pixels = stbi_load_from_memory(
		FileBytes.data(),
		static_cast<int>(FileBytes.size()),
		&W, &H, &OrigChannels,
		/*desired_channels=*/4);   // 항상 RGBA 4채널로 강제 — D3D11 8/8/8/8 포맷에 맞춤.
	if (!Pixels || W <= 0 || H <= 0)
	{
		UE_LOG("Failed to decode texture '%s': %s", FilePath.c_str(),
			stbi_failure_reason() ? stbi_failure_reason() : "unknown");
		if (Pixels) stbi_image_free(Pixels);
		return false;
	}

	// stb 는 8-bit RGBA 만 내려주므로 SRV 포맷은 8888 UNORM / sRGB 둘 중 하나.
	D3D11_TEXTURE2D_DESC Desc = {};
	Desc.Width              = static_cast<UINT>(W);
	Desc.Height             = static_cast<UINT>(H);
	Desc.MipLevels          = 1;
	Desc.ArraySize          = 1;
	Desc.Format             = (InColorSpace == ETextureColorSpace::SRGB)
		? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
		: DXGI_FORMAT_R8G8B8A8_UNORM;
	Desc.SampleDesc.Count   = 1;
	Desc.SampleDesc.Quality = 0;
	Desc.Usage              = D3D11_USAGE_DEFAULT;
	Desc.BindFlags          = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA InitData = {};
	InitData.pSysMem     = Pixels;
	InitData.SysMemPitch = static_cast<UINT>(W) * 4u;

	ID3D11Texture2D* Tex2D = nullptr;
	HRESULT hr = Device->CreateTexture2D(&Desc, &InitData, &Tex2D);
	stbi_image_free(Pixels);
	if (FAILED(hr) || !Tex2D)
	{
		UE_LOG("CreateTexture2D failed for '%s' (hr=0x%08X)", FilePath.c_str(), static_cast<unsigned>(hr));
		return false;
	}

	hr = Device->CreateShaderResourceView(Tex2D, nullptr, &SRV);
	if (FAILED(hr))
	{
		Tex2D->Release();
		UE_LOG("CreateShaderResourceView failed for '%s' (hr=0x%08X)", FilePath.c_str(), static_cast<unsigned>(hr));
		return false;
	}

	TrackedTextureMemory = MemoryStats::CalculateTextureMemory(Tex2D);
	if (TrackedTextureMemory > 0)
	{
		MemoryStats::AddTextureMemory(TrackedTextureMemory);
	}

	Width  = static_cast<uint32>(W);
	Height = static_cast<uint32>(H);
	Tex2D->Release();

	SourceFilePath = FilePath;
	ColorSpace     = InColorSpace;
	return true;
}
