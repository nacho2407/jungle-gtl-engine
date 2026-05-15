#include "SkeletonManager.h"

#include "Animation/Skeleton.h"
#include "Asset/AssetPackage.h"
#include "Core/Log.h"
#include "Object/Object.h"
#include "Platform/Paths.h"
#include "Serialization/WindowsArchive.h"

#include <algorithm>
#include <filesystem>

namespace
{
    static std::filesystem::path ResolveProjectPath(const FString& Path)
    {
        std::filesystem::path FullPath(FPaths::ToWide(Path));
        if (!FullPath.is_absolute())
        {
            FullPath = std::filesystem::path(FPaths::RootDir()) / FullPath;
        }
        return FullPath.lexically_normal();
    }

    static bool TryGetSourceFileState(const FString& SourcePath, uint64& OutTimestamp, uint64& OutFileSize)
    {
        std::filesystem::path FullPath = ResolveProjectPath(SourcePath);

        if (!std::filesystem::exists(FullPath) || !std::filesystem::is_regular_file(FullPath))
        {
            OutTimestamp = 0;
            OutFileSize  = 0;
            return false;
        }

        OutFileSize          = static_cast<uint64>(std::filesystem::file_size(FullPath));
        const auto WriteTime = std::filesystem::last_write_time(FullPath);
        OutTimestamp         = static_cast<uint64>(WriteTime.time_since_epoch().count());
        return true;
    }

    static FAssetImportMetadata MakeImportMetadata(const FString& SourcePath)
    {
        FAssetImportMetadata Metadata;
        Metadata.SourcePath = FPaths::MakeProjectRelative(SourcePath);
        TryGetSourceFileState(SourcePath, Metadata.SourceTimestamp, Metadata.SourceFileSize);
        return Metadata;
    }
}

FSkeletonManager& FSkeletonManager::Get()
{
    static FSkeletonManager Instance;
    return Instance;
}

FString FSkeletonManager::GetSkeletonPackagePath(const FString& SourcePath)
{
    std::filesystem::path ProjectRelative = std::filesystem::path(FPaths::ToWide(FPaths::MakeProjectRelative(SourcePath))).lexically_normal();

    std::filesystem::path AssetPath = std::filesystem::path(L"Content") / ProjectRelative;
    AssetPath.replace_filename(AssetPath.stem().wstring() + L"_Skeleton.uasset");

    std::filesystem::path FullAssetPath = std::filesystem::path(FPaths::RootDir()) / AssetPath;
    FPaths::CreateDir(FullAssetPath.parent_path().wstring());

    return FPaths::ToUtf8(AssetPath.generic_wstring());
}

USkeleton* FSkeletonManager::LoadSkeleton(const FString& PackagePath)
{
    const FString NormalizedPath = FPaths::MakeProjectRelative(PackagePath);

    auto It = SkeletonCaches.find(NormalizedPath);
    if (It != SkeletonCaches.end())
    {
        return It->second;
    }

    FWindowsBinReader Reader(NormalizedPath);
    if (!Reader.IsValid())
    {
        UE_LOG("Skeleton load failed: could not open file. Path=%s", NormalizedPath.c_str());
        return nullptr;
    }

    FAssetPackageHeader Header;
    Reader << Header;

    if (!Header.IsValid(EAssetPackageType::Skeleton))
    {
        UE_LOG("Skeleton load failed: invalid package header. Path=%s", NormalizedPath.c_str());
        return nullptr;
    }

    FAssetImportMetadata Metadata;
    Reader << Metadata;

    USkeleton* Skeleton = UObjectManager::Get().CreateObject<USkeleton>();
    Skeleton->Serialize(Reader);
    Skeleton->SetAssetPathFileName(NormalizedPath);

    if (!Reader.IsValid())
    {
        UE_LOG("Skeleton load failed: corrupted package. Path=%s", NormalizedPath.c_str());
        UObjectManager::Get().DestroyObject(Skeleton);
        return nullptr;
    }

    SkeletonCaches[NormalizedPath] = Skeleton;
    return Skeleton;
}

bool FSkeletonManager::SaveSkeleton(USkeleton* Skeleton, const FString& PackagePath, const FString& SourcePath)
{
    if (!Skeleton)
    {
        return false;
    }

    const FString NormalizedPath = FPaths::MakeProjectRelative(PackagePath);
    Skeleton->SetAssetPathFileName(NormalizedPath);

    FWindowsBinWriter Writer(NormalizedPath);
    if (!Writer.IsValid())
    {
        UE_LOG("Skeleton save failed: could not open file. Path=%s", NormalizedPath.c_str());
        return false;
    }

    FAssetPackageHeader Header;
    Header.Type = static_cast<uint32>(EAssetPackageType::Skeleton);

    FAssetImportMetadata Metadata = MakeImportMetadata(SourcePath);

    Writer << Header;
    Writer << Metadata;
    Skeleton->Serialize(Writer);

    if (!Writer.IsValid())
    {
        UE_LOG("Skeleton save failed: write failed. Path=%s", NormalizedPath.c_str());
        return false;
    }

    SkeletonCaches[NormalizedPath] = Skeleton;
    return true;
}
