// 엔진 영역의 세부 동작을 구현합니다.
#include "Resource/ResourceManager.h"
#include "Platform/Paths.h"
#include "SimpleJSON/json.hpp"

#include <fstream>
#include <filesystem>
#include <d3d11.h>
#include "DDSTextureLoader.h"
#include "WICTextureLoader.h"
#include "Editor/UI/EditorConsolePanel.h"
#include "Profiling/MemoryStats.h"

namespace ResourceKey
{
constexpr const char* Font = "Font";
constexpr const char* Particle = "Particle";
constexpr const char* Texture = "Texture";
constexpr const char* Path = "Path";
constexpr const char* Columns = "Columns";
constexpr const char* Rows = "Rows";
} // namespace ResourceKey

void FResourceManager::LoadFromFile(const FString& Path, ID3D11Device* InDevice)
{
    using namespace json;

    FontResources.clear();
    ParticleResources.clear();
    TextureResources.clear();

    std::ifstream File(std::filesystem::path(FPaths::ToWide(Path)));
    checkf(File.is_open(), "Failed to open Settings/Resource.ini");

    FString Content((std::istreambuf_iterator<char>(File)),
                    std::istreambuf_iterator<char>());
    checkf(!Content.empty(), "Settings/Resource.ini is empty");

    JSON Root = JSON::Load(Content);

    // Font — { "Name": { "Path": "...", "Columns": 16, "Rows": 16 } }
    if (Root.hasKey(ResourceKey::Font))
    {
        JSON FontSection = Root[ResourceKey::Font];
        for (auto& Pair : FontSection.ObjectRange())
        {
            JSON Entry = Pair.second;

            checkf(Entry.hasKey(ResourceKey::Path), "Font entry is missing Path");
            checkf(Entry.hasKey(ResourceKey::Columns), "Font entry is missing Columns");
            checkf(Entry.hasKey(ResourceKey::Rows), "Font entry is missing Rows");

            FFontResource Resource;
            Resource.Name = FName(Pair.first.c_str());
            Resource.Path = Entry[ResourceKey::Path].ToString();
            Resource.Columns = static_cast<uint32>(Entry[ResourceKey::Columns].ToInt());
            Resource.Rows = static_cast<uint32>(Entry[ResourceKey::Rows].ToInt());
            Resource.SRV = nullptr;

            checkf(!Resource.Path.empty(), "Font resource path is empty");
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

            checkf(Entry.hasKey(ResourceKey::Path), "Particle entry is missing Path");
            checkf(Entry.hasKey(ResourceKey::Columns), "Particle entry is missing Columns");
            checkf(Entry.hasKey(ResourceKey::Rows), "Particle entry is missing Rows");

            FParticleResource Resource;
            Resource.Name = FName(Pair.first.c_str());
            Resource.Path = Entry[ResourceKey::Path].ToString();
            Resource.Columns = static_cast<uint32>(Entry[ResourceKey::Columns].ToInt());
            Resource.Rows = static_cast<uint32>(Entry[ResourceKey::Rows].ToInt());
            Resource.SRV = nullptr;

            checkf(!Resource.Path.empty(), "Particle resource path is empty");
            ParticleResources[Pair.first] = Resource;
        }
    }

    // Texture — { "Name": { "Path": "..." } }  (Columns/Rows는 항상 1)
    if (Root.hasKey(ResourceKey::Texture))
    {
        JSON TextureSection = Root[ResourceKey::Texture];
        for (auto& Pair : TextureSection.ObjectRange())
        {
            JSON Entry = Pair.second;

            checkf(Entry.hasKey(ResourceKey::Path), "Texture entry is missing Path");

            FTextureResource Resource;
            Resource.Name = FName(Pair.first.c_str());
            Resource.Path = Entry[ResourceKey::Path].ToString();
            Resource.Columns = 1;
            Resource.Rows = 1;
            Resource.SRV = nullptr;

            checkf(!Resource.Path.empty(), "Texture resource path is empty");
            TextureResources[Pair.first] = Resource;
        }
    }

    checkf(LoadGPUResources(InDevice), "Failed to create GPU resources from Settings/Resource.ini");
    UE_LOG("Complete Load Resources!");
}

bool FResourceManager::LoadGPUResources(ID3D11Device* Device)
{
    if (!Device)
    {
        return false;
    }

    auto LoadSRV = [&](FTextureAtlasResource& Resource) -> bool
    {
        if (Resource.SRV)
        {
            if (Resource.TrackedMemoryBytes > 0)
            {
                MemoryStats::SubTextureMemory(Resource.TrackedMemoryBytes);
                Resource.TrackedMemoryBytes = 0;
            }
            Resource.SRV->Release();
            Resource.SRV = nullptr;
        }

        std::wstring FullPath = FPaths::Combine(FPaths::RootDir(), FPaths::ToWide(Resource.Path));

        // 확장자에 따라 DDS / WIC 로더 분기
        std::filesystem::path Ext = std::filesystem::path(Resource.Path).extension();
        FString ExtStr = Ext.string();
        for (char& c : ExtStr)
            c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));

        HRESULT hr;
        if (ExtStr == ".dds")
        {
            hr = DirectX::CreateDDSTextureFromFileEx(
                Device,
                FullPath.c_str(),
                0,
                D3D11_USAGE_IMMUTABLE,
                D3D11_BIND_SHADER_RESOURCE,
                0, 0,
                DirectX::DDS_LOADER_DEFAULT,
                nullptr,
                &Resource.SRV);
        }
        else
        {
            // .png/.jpg/.bmp/.tga 등 — WIC 경유
            hr = DirectX::CreateWICTextureFromFileEx(
                Device,
                FullPath.c_str(),
                0,
                D3D11_USAGE_IMMUTABLE,
                D3D11_BIND_SHADER_RESOURCE,
                0, 0,
                // 이 버전의 DirectXTK 에는 PREMULTIPLY_ALPHA 플래그가 없다.
                // straight-alpha 보간으로 생기는 검은 헤일로는 PS 측 알파 컷오프(0.5) 로 회피.
                DirectX::WIC_LOADER_FORCE_RGBA32,
                nullptr,
                &Resource.SRV);
        }
        if (FAILED(hr) || !Resource.SRV)
        {
            return false;
        }

        ID3D11Resource* TextureResource = nullptr;
        Resource.SRV->GetResource(&TextureResource);
        Resource.TrackedMemoryBytes = MemoryStats::CalculateTextureMemory(TextureResource);
        if (TextureResource)
        {
            TextureResource->Release();
        }

        if (Resource.TrackedMemoryBytes > 0)
        {
            MemoryStats::AddTextureMemory(Resource.TrackedMemoryBytes);
        }

        return true;
    };

    for (auto& [Key, Resource] : FontResources)
    {
        if (!LoadSRV(Resource))
            return false;
    }

    for (auto& [Key, Resource] : ParticleResources)
    {
        if (!LoadSRV(Resource))
            return false;
    }

    for (auto& [Key, Resource] : TextureResources)
    {
        if (!LoadSRV(Resource))
            return false;
    }

    return true;
}

void FResourceManager::ReleaseGPUResources()
{
    for (auto& [Key, Resource] : FontResources)
    {
        if (Resource.TrackedMemoryBytes > 0)
        {
            MemoryStats::SubTextureMemory(Resource.TrackedMemoryBytes);
            Resource.TrackedMemoryBytes = 0;
        }
        if (Resource.SRV)
        {
            Resource.SRV->Release();
            Resource.SRV = nullptr;
        }
    }
    for (auto& [Key, Resource] : ParticleResources)
    {
        if (Resource.TrackedMemoryBytes > 0)
        {
            MemoryStats::SubTextureMemory(Resource.TrackedMemoryBytes);
            Resource.TrackedMemoryBytes = 0;
        }
        if (Resource.SRV)
        {
            Resource.SRV->Release();
            Resource.SRV = nullptr;
        }
    }
    for (auto& [Key, Resource] : TextureResources)
    {
        if (Resource.TrackedMemoryBytes > 0)
        {
            MemoryStats::SubTextureMemory(Resource.TrackedMemoryBytes);
            Resource.TrackedMemoryBytes = 0;
        }
        if (Resource.SRV)
        {
            Resource.SRV->Release();
            Resource.SRV = nullptr;
        }
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
    Resource.Name = FontName;
    Resource.Path = InPath;
    Resource.Columns = Columns;
    Resource.Rows = Rows;
    Resource.SRV = nullptr;
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
    Resource.Name = ParticleName;
    Resource.Path = InPath;
    Resource.Columns = Columns;
    Resource.Rows = Rows;
    Resource.SRV = nullptr;
    ParticleResources[ParticleName.ToString()] = Resource;
}

TArray<FString> FResourceManager::GetFontNames() const
{
    TArray<FString> Names;
    Names.reserve(FontResources.size());
    for (const auto& [Key, _] : FontResources)
    {
        Names.push_back(Key);
    }
    return Names;
}

TArray<FString> FResourceManager::GetParticleNames() const
{
    TArray<FString> Names;
    Names.reserve(ParticleResources.size());
    for (const auto& [Key, _] : ParticleResources)
    {
        Names.push_back(Key);
    }
    return Names;
}

// --- Texture ---
FTextureResource* FResourceManager::FindTexture(const FName& TextureName)
{
    auto It = TextureResources.find(TextureName.ToString());
    return (It != TextureResources.end()) ? &It->second : nullptr;
}

const FTextureResource* FResourceManager::FindTexture(const FName& TextureName) const
{
    auto It = TextureResources.find(TextureName.ToString());
    return (It != TextureResources.end()) ? &It->second : nullptr;
}

void FResourceManager::RegisterTexture(const FName& TextureName, const FString& InPath)
{
    FTextureResource Resource;
    Resource.Name = TextureName;
    Resource.Path = InPath;
    Resource.Columns = 1;
    Resource.Rows = 1;
    Resource.SRV = nullptr;
    TextureResources[TextureName.ToString()] = Resource;
}

TArray<FString> FResourceManager::GetTextureNames() const
{
    TArray<FString> Names;
    Names.reserve(TextureResources.size());
    for (const auto& [Key, Resource] : TextureResources)
    {
        if (FPaths::IsEditorAssetPath(Resource.Path))
        {
            continue;
        }
        Names.push_back(Key);
    }
    return Names;
}

TArray<FString> FResourceManager::GetEditorTextureNames() const
{
    TArray<FString> Names;
    Names.reserve(TextureResources.size());
    for (const auto& [Key, Resource] : TextureResources)
    {
        if (!FPaths::IsEditorAssetPath(Resource.Path))
        {
            continue;
        }
        Names.push_back(Key);
    }
    return Names;
}
