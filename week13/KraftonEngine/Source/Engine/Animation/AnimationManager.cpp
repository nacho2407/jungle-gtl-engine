#include "AnimationManager.h"

#include "Animation/AnimSequence.h"
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

    static FString SanitizeAssetName(const FString& Name)
    {
        FString Result = Name;

        for (char& C : Result)
        {
            const bool bValid = (C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z') || (C >= '0' && C <= '9') || C == '_' || C == '-';

            if (!bValid)
            {
                C = '_';
            }
        }

        if (Result.empty())
        {
            Result = "Anim";
        }

        return Result;
    }
}

FAnimationManager& FAnimationManager::Get()
{
    static FAnimationManager Instance;
    return Instance;
}

FString FAnimationManager::GetAnimationPackagePath(const FString& SourcePath, const FString& AnimationName)
{
    std::filesystem::path ProjectRelative = std::filesystem::path(FPaths::ToWide(FPaths::MakeProjectRelative(SourcePath))).lexically_normal();

    const FString SafeAnimName = SanitizeAssetName(AnimationName);

    std::filesystem::path AssetPath = std::filesystem::path(L"Content") / ProjectRelative;
    AssetPath.replace_filename(AssetPath.stem().wstring() + L"_" + FPaths::ToWide(SafeAnimName) + L".uasset");

    std::filesystem::path FullAssetPath = std::filesystem::path(FPaths::RootDir()) / AssetPath;
    FPaths::CreateDir(FullAssetPath.parent_path().wstring());

    return FPaths::ToUtf8(AssetPath.generic_wstring());
}

UAnimSequence* FAnimationManager::LoadAnimation(const FString& PackagePath)
{
    const FString NormalizedPath = FPaths::MakeProjectRelative(PackagePath);

    auto It = AnimationCaches.find(NormalizedPath);
    if (It != AnimationCaches.end())
    {
        return It->second;
    }

    FWindowsBinReader Reader(NormalizedPath);
    if (!Reader.IsValid())
    {
        UE_LOG("Animation load failed: could not open file. Path=%s", NormalizedPath.c_str());
        return nullptr;
    }

    FAssetPackageHeader Header;
    Reader << Header;

    if (!Header.IsValid(EAssetPackageType::AnimSequence))
    {
        UE_LOG("Animation load failed: invalid package header. Path=%s", NormalizedPath.c_str());
        return nullptr;
    }

    FAssetImportMetadata Metadata;
    Reader << Metadata;

    UAnimSequence* Sequence = UObjectManager::Get().CreateObject<UAnimSequence>();
    Sequence->Serialize(Reader);
    Sequence->SetAssetPathFileName(NormalizedPath);

    if (!Reader.IsValid())
    {
        UE_LOG("Animation load failed: corrupted package. Path=%s", NormalizedPath.c_str());
        UObjectManager::Get().DestroyObject(Sequence);
        return nullptr;
    }

    AnimationCaches[NormalizedPath] = Sequence;
    return Sequence;
}

bool FAnimationManager::SaveAnimation(UAnimSequence* Sequence, const FString& PackagePath, const FString& SourcePath)
{
    if (!Sequence)
    {
        return false;
    }

    const FString NormalizedPath = FPaths::MakeProjectRelative(PackagePath);
    Sequence->SetAssetPathFileName(NormalizedPath);

    FWindowsBinWriter Writer(NormalizedPath);
    if (!Writer.IsValid())
    {
        UE_LOG("Animation save failed: could not open file. Path=%s", NormalizedPath.c_str());
        return false;
    }

    FAssetPackageHeader Header;
    Header.Type = static_cast<uint32>(EAssetPackageType::AnimSequence);

    FAssetImportMetadata Metadata = MakeImportMetadata(SourcePath);

    Writer << Header;
    Writer << Metadata;
    Sequence->Serialize(Writer);

    if (!Writer.IsValid())
    {
        UE_LOG("Animation save failed: write failed. Path=%s", NormalizedPath.c_str());
        return false;
    }

    AnimationCaches[NormalizedPath] = Sequence;
    return true;
}
