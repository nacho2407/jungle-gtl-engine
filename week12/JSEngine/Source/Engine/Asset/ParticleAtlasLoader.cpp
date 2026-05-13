#include "ParticleAtlasLoader.h"

#include "Core/Paths.h"
#include "Render/Resource/Texture.h"

#include "DDSTextureLoader.h"
#include "WICTextureLoader.h"

#include <filesystem>

bool FParticleAtlasLoader::Load(const FName& ParticleName, const FString& Path, uint32 Columns, uint32 Rows,
	ID3D11Device* Device, FParticleResource& OutResource) const
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
			OutResource.Texture->GetAddressOfSRV());
	}
	else
	{
		Hr = DirectX::CreateWICTextureFromFile(Device, FullPath.c_str(), nullptr, OutResource.Texture->GetAddressOfSRV());
	}

	if (FAILED(Hr))
	{
		return false;
	}

	OutResource.Name = ParticleName;
	OutResource.Path = Path;
	OutResource.Columns = Columns;
	OutResource.Rows = Rows;
	return true;
}

bool FParticleAtlasLoader::SupportsExtension(const FString& Extension) const
{
	return Extension == ".dds" || Extension == "dds"
		|| Extension == ".png" || Extension == "png"
		|| Extension == ".jpg" || Extension == "jpg"
		|| Extension == ".jpeg" || Extension == "jpeg";
}

FString FParticleAtlasLoader::GetLoaderName() const
{
	return "FParticleAtlasLoader";
}
