#include "ParticleSystemManager.h"

#include <algorithm>
#include <filesystem>

#include "Particle/ParticleSystem.h"
#include "Asset/AssetPackage.h"
#include "Platform/Paths.h"
#include "Serialization/WindowsArchive.h"
#include "Object/Reflection/ObjectFactory.h"

UParticleSystem* FParticleSystemManager::Load(const FString& Path)
{
    const FString NormalizedPath = FPaths::MakeProjectRelative(Path);

    auto It = LoadedParticleSystems.find(NormalizedPath);
    if (It != LoadedParticleSystems.end())
    {
        return It->second;
    }

    if (!FAssetPackage::IsAssetPackagePath(NormalizedPath)) return nullptr;

    FWindowsBinReader Ar(NormalizedPath);
    if (!Ar.IsValid()) return nullptr;

    FAssetPackageHeader Header;
    Ar << Header;
    if (!Header.IsValid(EAssetPackageType::ParticleSystem)) return nullptr;

    FAssetImportMetadata Metadata;
    Ar << Metadata;

    UParticleSystem* NewAsset = UObjectManager::Get().CreateObject<UParticleSystem>();
    NewAsset->Serialize(Ar);

    if (!Ar.IsValid())
    {
        UObjectManager::Get().DestroyObject(NewAsset);
        return nullptr;
    }

    NewAsset->SetSourcePath(NormalizedPath);
    LoadedParticleSystems.emplace(NormalizedPath, NewAsset);
    return NewAsset;
}

UParticleSystem* FParticleSystemManager::Find(const FString& Path) const
{
    const FString NormalizedPath = FPaths::MakeProjectRelative(Path);
    auto          It             = LoadedParticleSystems.find(NormalizedPath);
    return It != LoadedParticleSystems.end() ? It->second : nullptr;
}

bool FParticleSystemManager::Save(UParticleSystem* Asset)
{
    if (!Asset)
    {
        return false;
    }
    const FString& Path = Asset->GetSourcePath();
    if (Path.empty())
    {
        return false;
    }

    FWindowsBinWriter Ar(FPaths::MakeProjectRelative(Path));
    if (!Ar.IsValid())
    {
        return false;
    }

    FAssetPackageHeader Header;
    Header.Type = static_cast<uint32>(EAssetPackageType::ParticleSystem);

    FAssetImportMetadata Metadata;
    Ar << Header;
    Ar << Metadata;
    Asset->Serialize(Ar);
    return Ar.IsValid();
}

void FParticleSystemManager::RefreshAvailableParticleSystems()
{
    AvailableParticleSystemFiles.clear();
    
    const std::filesystem::path ContentRoot = std::filesystem::path(FPaths::RootDir()) / L"Content";

    if (!std::filesystem::exists(ContentRoot)) return;

    const std::filesystem::path ProjectRoot(FPaths::RootDir());

    for (const auto& Entry : std::filesystem::recursive_directory_iterator(ContentRoot))
    {
        if (!Entry.is_regular_file()) continue;

        std::wstring Ext = Entry.path().extension().wstring();
        std::transform(Ext.begin(), Ext.end(), Ext.begin(), ::towlower);
        if (Ext != L".uasset") continue;

        const FString RelPath = FPaths::ToUtf8(Entry.path().lexically_relative(ProjectRoot).generic_wstring());

        FAssetImportMetadata Metadata;
        if (!FAssetPackage::ReadMetadata(RelPath, EAssetPackageType::ParticleSystem, Metadata))
        {
            continue;
        }

        FAssetListItem Item;
        Item.DisplayName = FPaths::ToUtf8(Entry.path().stem().wstring());
        Item.FullPath    = RelPath;

        AvailableParticleSystemFiles.push_back(std::move(Item));
    }
}
