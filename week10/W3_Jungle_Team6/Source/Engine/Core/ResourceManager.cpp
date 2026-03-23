#include "Core/ResourceManager.h"
#include "Core/Paths.h"
#include "SimpleJSON/json.hpp"

#include <fstream>
#include <filesystem>
#include "DDSTextureLoader.h"
#include "UI/EditorConsoleWidget.h"

namespace ResourceKey
{
	constexpr const char* Font     = "Font";
	constexpr const char* Particle = "Particle";
	constexpr const char* Path     = "Path";
	constexpr const char* Columns  = "Columns";
	constexpr const char* Rows     = "Rows";
}

void FResourceManager::LoadFromFile(const FString& Path, ID3D11Device* InDevice)
{
	using namespace json;

	std::ifstream File(std::filesystem::path(FPaths::ToWide(Path)));
	if (!File.is_open())
	{
		return;
	}

	FString Content((std::istreambuf_iterator<char>(File)),
		std::istreambuf_iterator<char>());

	JSON Root = JSON::Load(Content);

	// Font — { "Name": { "Path": "...", "Columns": 16, "Rows": 16 } }
	if (Root.hasKey(ResourceKey::Font))
	{
		JSON FontSection = Root[ResourceKey::Font];
		for (auto& Pair : FontSection.ObjectRange())
		{
			JSON Entry = Pair.second;
			FFontResource Resource;
			Resource.Name    = FName(Pair.first.c_str());
			Resource.Path    = Entry[ResourceKey::Path].ToString();
			Resource.Columns = static_cast<uint32>(Entry[ResourceKey::Columns].ToInt());
			Resource.Rows    = static_cast<uint32>(Entry[ResourceKey::Rows].ToInt());
			Resource.SRV     = nullptr;
			FontResources[Pair.first] = Resource;
		}
	}

	// Particle — { "Name": { "Path": "...", "Columns": 6, "Rows": 6 } }
	if (Root.hasKey(ResourceKey::Particle))
	{
		JSON ParticleSection = Root[ResourceKey::Particle];
		for (auto& Pair : ParticleSection.ObjectRange())
		{
			JSON Entry = Pair.second;
			FParticleResource Resource;
			Resource.Name    = FName(Pair.first.c_str());
			Resource.Path    = Entry[ResourceKey::Path].ToString();
			Resource.Columns = static_cast<uint32>(Entry[ResourceKey::Columns].ToInt());
			Resource.Rows    = static_cast<uint32>(Entry[ResourceKey::Rows].ToInt());
			Resource.SRV     = nullptr;
			ParticleResources[Pair.first] = Resource;
		}
	}

	if (LoadGPUResources(InDevice))
	{
		UE_LOG("Complete Load Resources!");
	}
	else
	{
		UE_LOG("Failed to Load Resources...");
	}
}

bool FResourceManager::LoadGPUResources(ID3D11Device* Device)
{
	if (!Device)
	{
		return false;
	}

	auto LoadSRV = [&](FTextureAtlasResource& Resource) -> bool
	{
		std::wstring FullPath = FPaths::Combine(FPaths::RootDir(), FPaths::ToWide(Resource.Path));
		HRESULT hr = DirectX::CreateDDSTextureFromFileEx(
			Device,
			FullPath.c_str(),
			0,
			D3D11_USAGE_IMMUTABLE,
			D3D11_BIND_SHADER_RESOURCE,
			0, 0,
			DirectX::DDS_LOADER_DEFAULT,
			nullptr,
			&Resource.SRV
		);
		return SUCCEEDED(hr);
	};

	for (auto& [Key, Resource] : FontResources)
	{
		if (!LoadSRV(Resource)) return false;
	}

	for (auto& [Key, Resource] : ParticleResources)
	{
		if (!LoadSRV(Resource)) return false;
	}

	return true;
}

void FResourceManager::ReleaseGPUResources()
{
	for (auto& [Key, Resource] : FontResources)
	{
		if (Resource.SRV) { Resource.SRV->Release(); Resource.SRV = nullptr; }
	}
	for (auto& [Key, Resource] : ParticleResources)
	{
		if (Resource.SRV) { Resource.SRV->Release(); Resource.SRV = nullptr; }
	}
}

// --- Font ---
FFontResource* FResourceManager::FindFont(const FName& FontName)
{
	auto It = FontResources.find(FontName.ToString());
	return (It != FontResources.end()) ? &It->second : nullptr;
}

const FFontResource* FResourceManager::FindFont(const FName& FontName) const
{
	auto It = FontResources.find(FontName.ToString());
	return (It != FontResources.end()) ? &It->second : nullptr;
}

void FResourceManager::RegisterFont(const FName& FontName, const FString& InPath, uint32 Columns, uint32 Rows)
{
	FFontResource Resource;
	Resource.Name    = FontName;
	Resource.Path    = InPath;
	Resource.Columns = Columns;
	Resource.Rows    = Rows;
	Resource.SRV     = nullptr;
	FontResources[FontName.ToString()] = Resource;
}

// --- Particle ---
FParticleResource* FResourceManager::FindParticle(const FName& ParticleName)
{
	auto It = ParticleResources.find(ParticleName.ToString());
	return (It != ParticleResources.end()) ? &It->second : nullptr;
}

const FParticleResource* FResourceManager::FindParticle(const FName& ParticleName) const
{
	auto It = ParticleResources.find(ParticleName.ToString());
	return (It != ParticleResources.end()) ? &It->second : nullptr;
}

void FResourceManager::RegisterParticle(const FName& ParticleName, const FString& InPath, uint32 Columns, uint32 Rows)
{
	FParticleResource Resource;
	Resource.Name    = ParticleName;
	Resource.Path    = InPath;
	Resource.Columns = Columns;
	Resource.Rows    = Rows;
	Resource.SRV     = nullptr;
	ParticleResources[ParticleName.ToString()] = Resource;
}
