#include "FontAtlasLoader.h"

#include "Core/Paths.h"

#include "DDSTextureLoader.h"
#include "WICTextureLoader.h"

#include <algorithm>
#include <filesystem>

bool FFontAtlasLoader::Load(const FName& FontName, const FString& Path, uint32 Columns, uint32 Rows,
	ID3D11Device* Device, FFontResource& OutResource) const
{
	if (!Device || Path.empty())
	{
		return false;
	}

	std::wstring FullPath = FPaths::Combine(FPaths::RootDir(), FPaths::ToWide(Path));
	const std::wstring Extension = std::filesystem::path(FullPath).extension().wstring();

	HRESULT Hr = E_FAIL;

	if (Extension == L".dds" || Extension == L".DDS")
	{
		Hr = DirectX::CreateDDSTextureFromFileEx(
			Device,
			FullPath.c_str(),
			0,
			D3D11_USAGE_IMMUTABLE,
			D3D11_BIND_SHADER_RESOURCE,
			0,
			0,
			DirectX::DDS_LOADER_DEFAULT,
			nullptr,
			OutResource.SRV.ReleaseAndGetAddressOf());
	}
	else
	{
		Hr = DirectX::CreateWICTextureFromFile(Device, FullPath.c_str(), nullptr, OutResource.SRV.ReleaseAndGetAddressOf());
	}

	if (FAILED(Hr))
	{
		return false;
	}

	OutResource.Name = FontName;
	OutResource.Path = Path;
	OutResource.Columns = Columns;
	OutResource.Rows = Rows;
	return true;
}

bool FFontAtlasLoader::SupportsExtension(const FString& Extension) const
{
	return Extension == ".dds" || Extension == "dds"
		|| Extension == ".png" || Extension == "png"
		|| Extension == ".jpg" || Extension == "jpg"
		|| Extension == ".jpeg" || Extension == "jpeg";
}

FString FFontAtlasLoader::GetLoaderName() const
{
	return "FFontAtlasLoader";
}
