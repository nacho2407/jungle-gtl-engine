#include "Texture.h"
#include "Core/Paths.h"

#include "DDSTextureLoader.h"
#include "WICTextureLoader.h"
#include "UI/EditorConsoleWidget.h"

DEFINE_CLASS(UTexture, UObject)

bool UTexture::LoadFromFile(const FString& InFilePath, ID3D11Device* InDevice)
{
	FilePath = InFilePath;

	std::wstring FullPath = FPaths::Combine(FPaths::RootDir(), FPaths::ToWide(InFilePath));

	HRESULT hr;
	if (FullPath.size() >= 4 && FullPath.substr(FullPath.size() - 4) == L".dds")
	{
		hr = DirectX::CreateDDSTextureFromFile(InDevice, FullPath.c_str(), nullptr, &TextureData.SRV);
	}
	else
	{
		hr = DirectX::CreateWICTextureFromFile(InDevice, FullPath.c_str(), nullptr, &TextureData.SRV);
	}

	if (FAILED(hr))
	{
		UE_LOG("Failed to load texture: %s", InFilePath.c_str());
		return false;
	}

	return true;
}