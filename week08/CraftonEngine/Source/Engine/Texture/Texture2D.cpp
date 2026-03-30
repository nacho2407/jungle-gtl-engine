#include "Texture/Texture2D.h"
#include "Object/ObjectFactory.h"
#include "UI/EditorConsoleWidget.h"
#include "Platform/Paths.h"
#include "WICTextureLoader.h"

#include <d3d11.h>
#include <filesystem>

IMPLEMENT_CLASS(UTexture2D, UObject)

std::map<FString, UTexture2D*> UTexture2D::TextureCache;

UTexture2D::~UTexture2D()
{
	if (SRV)
	{
		SRV->Release();
		SRV = nullptr;
	}

	// 캐시에서 제거
	auto It = TextureCache.find(SourceFilePath);
	if (It != TextureCache.end() && It->second == this)
	{
		TextureCache.erase(It);
	}
}

UTexture2D* UTexture2D::LoadFromFile(const FString& FilePath, ID3D11Device* Device)
{
	if (FilePath.empty() || !Device) return nullptr;

	// 캐시 히트
	auto It = TextureCache.find(FilePath);
	if (It != TextureCache.end())
	{
		return It->second;
	}

	// 새 UTexture2D 생성
	UTexture2D* Texture = UObjectManager::Get().CreateObject<UTexture2D>();
	if (!Texture->LoadInternal(FilePath, Device))
	{
		UObjectManager::Get().DestroyObject(Texture);
		return nullptr;
	}

	TextureCache[FilePath] = Texture;
	return Texture;
}

bool UTexture2D::LoadInternal(const FString& FilePath, ID3D11Device* Device)
{
	//std::filesystem::path TexPath(FilePath);
	//std::wstring WidePath = TexPath.wstring();
	std::wstring WidePath = FPaths::ToWide(FilePath);

	ID3D11Resource* Resource = nullptr;
	HRESULT hr = DirectX::CreateWICTextureFromFile(
		Device, WidePath.c_str(), &Resource, &SRV);

	if (FAILED(hr))
	{
		UE_LOG("Failed to load texture: %s", FilePath.c_str());
		return false;
	}

	// 텍스처 크기 추출
	if (Resource)
	{
		ID3D11Texture2D* Tex2D = nullptr;
		if (SUCCEEDED(Resource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&Tex2D)))
		{
			D3D11_TEXTURE2D_DESC Desc;
			Tex2D->GetDesc(&Desc);
			Width = Desc.Width;
			Height = Desc.Height;
			Tex2D->Release();
		}
		Resource->Release();
	}

	SourceFilePath = FilePath;
	return true;
}
