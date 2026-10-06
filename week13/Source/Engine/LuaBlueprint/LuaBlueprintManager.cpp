#include "LuaBlueprint/LuaBlueprintManager.h"

#include "Asset/AssetPackage.h"
#include "LuaBlueprint/LuaBlueprintAsset.h"
#include "Object/GarbageCollection.h"
#include "Platform/Paths.h"
#include "Serialization/WindowsArchive.h"

#include <algorithm>
#include <filesystem>

ULuaBlueprintAsset* FLuaBlueprintManager::Load(const FString& Path)
{
    const FString NormalizedPath = FPaths::MakeProjectRelative(Path);

    auto It = LoadedBlueprints.find(NormalizedPath);
    if (It != LoadedBlueprints.end())
    {
        return It->second;
    }

    if (!FAssetPackage::IsAssetPackagePath(NormalizedPath)) return nullptr;

    FWindowsBinReader Ar(NormalizedPath);
    if (!Ar.IsValid()) return nullptr;

    FAssetPackageHeader Header;
    Ar << Header;
    if (!Header.IsValid(EAssetPackageType::LuaBlueprint)) return nullptr;

    FAssetImportMetadata Metadata;
    Ar << Metadata;

    ULuaBlueprintAsset* NewAsset = UObjectManager::Get().CreateObject<ULuaBlueprintAsset>();
    NewAsset->Serialize(Ar);
    if (!Ar.IsValid())
    {
        UObjectManager::Get().DestroyObject(NewAsset);
        return nullptr;
    }

    NewAsset->SetSourcePath(NormalizedPath);
    if (NewAsset->IsCompileDirty())
    {
        NewAsset->Compile();
    }
    LoadedBlueprints.emplace(NormalizedPath, NewAsset);
    return NewAsset;
}

ULuaBlueprintAsset* FLuaBlueprintManager::Find(const FString& Path) const
{
    const FString NormalizedPath = FPaths::MakeProjectRelative(Path);
    auto          It             = LoadedBlueprints.find(NormalizedPath);
    return It != LoadedBlueprints.end() ? It->second : nullptr;
}

bool FLuaBlueprintManager::Save(ULuaBlueprintAsset* Asset)
{
    if (!Asset) return false;
    const FString& Path = Asset->GetSourcePath();
    if (Path.empty()) return false;

    if (Asset->IsCompileDirty())
    {
        Asset->Compile();
    }

    FWindowsBinWriter Ar(FPaths::MakeProjectRelative(Path));
    if (!Ar.IsValid()) return false;

    FAssetPackageHeader Header;
    Header.Type = static_cast<uint32>(EAssetPackageType::LuaBlueprint);
    FAssetImportMetadata Metadata;

    Ar << Header;
    Ar << Metadata;
    Asset->Serialize(Ar);
    return Ar.IsValid();
}

void FLuaBlueprintManager::RefreshAvailableBlueprints()
{
    const std::filesystem::path ContentRoot = std::filesystem::path(FPaths::RootDir()) / L"Content";
    if (!std::filesystem::exists(ContentRoot)) return;

    const std::filesystem::path ProjectRoot(FPaths::RootDir());
    AvailableBlueprintFiles.clear();

    for (const auto& Entry : std::filesystem::recursive_directory_iterator(ContentRoot))
    {
        if (!Entry.is_regular_file()) continue;

        std::wstring Ext = Entry.path().extension().wstring();
        std::transform(Ext.begin(), Ext.end(), Ext.begin(), ::towlower);
        if (Ext != L".uasset") continue;

        const FString        RelPath = FPaths::ToUtf8(Entry.path().lexically_relative(ProjectRoot).generic_wstring());
        FAssetImportMetadata Metadata;
        if (!FAssetPackage::ReadMetadata(RelPath, EAssetPackageType::LuaBlueprint, Metadata))
        {
            continue;
        }

        FAssetListItem Item;
        Item.DisplayName = FPaths::ToUtf8(Entry.path().stem().wstring());
        Item.FullPath    = RelPath;
        AvailableBlueprintFiles.push_back(std::move(Item));
    }
}

void FLuaBlueprintManager::AddReferencedObjects(FReferenceCollector& Collector)
{
    for (auto& Pair : LoadedBlueprints)
    {
        Collector.AddReferencedObject(Pair.second);
    }
}

void FLuaBlueprintManager::ClearCache()
{
    LoadedBlueprints.clear();
    AvailableBlueprintFiles.clear();
}
