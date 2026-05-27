#include "SkeletonManager.h"

#include "Animation/Skeleton/Skeleton.h"
#include "Asset/AssetPackage.h"
#include "Core/Logging/Log.h"
#include "Object/Object.h"
#include "Platform/Paths.h"
#include "Serialization/WindowsArchive.h"
#include "Object/ReferenceCollector.h"

#include <algorithm>
#include <cstdio>
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

    static void HashByte(uint64& Hash, uint8 Value)
    {
        static constexpr uint64 Prime = 1099511628211ull;
        Hash                          ^= static_cast<uint64>(Value);
        Hash                          *= Prime;
    }

    static void HashString(uint64& Hash, const FString& Value)
    {
        for (char C : Value)
        {
            HashByte(Hash, static_cast<uint8>(C));
        }
        HashByte(Hash, 0xff);
    }

    static void HashInt32(uint64& Hash, int32 Value)
    {
        for (int32 Shift = 0; Shift < 32; Shift += 8)
        {
            HashByte(Hash, static_cast<uint8>((static_cast<uint32>(Value) >> Shift) & 0xffu));
        }
    }

    static FString Hex64(uint64 Value)
    {
        char Buffer[32] = {};
        std::snprintf(Buffer, sizeof(Buffer), "%016llx", static_cast<unsigned long long>(Value));
        return FString(Buffer);
    }

    static FString GetDisplayNameFromPath(const std::filesystem::path& Path)
    {
        return FPaths::ToUtf8(Path.stem().generic_wstring());
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

    // SourcePath 가 이미 Content/ 하위면 그대로 — 그렇지 않으면 (구 Data/ root 호환) prefix.
    std::filesystem::path AssetPath = (!ProjectRelative.empty() && ProjectRelative.begin()->wstring() == L"Content")
        ? ProjectRelative
        : (std::filesystem::path(L"Content") / ProjectRelative);
    AssetPath.replace_filename(AssetPath.stem().wstring() + L"_Skeleton.uasset");

    std::filesystem::path FullAssetPath = std::filesystem::path(FPaths::RootDir()) / AssetPath;
    FPaths::CreateDir(FullAssetPath.parent_path().wstring());

    return FPaths::ToUtf8(AssetPath.generic_wstring());
}

FString FSkeletonManager::BuildCompatibilitySignature(const FReferenceSkeleton& RefSkeleton)
{
    uint64 Hash = 14695981039346656037ull;
    HashString(Hash, "SkeletonStructureV1");
    HashInt32(Hash, RefSkeleton.GetNumBones());

    for (const FReferenceBone& Bone : RefSkeleton.Bones)
    {
        HashString(Hash, Bone.Name);
        HashInt32(Hash, Bone.ParentIndex);
    }

    return FString("SIG-") + Hex64(Hash);
}

FString FSkeletonManager::MakeSkeletonAssetGuid(const FString& PackagePath, const FString& CompatibilitySignature)
{
    uint64 Hash = 14695981039346656037ull;
    HashString(Hash, "SkeletonAssetV1");
    HashString(Hash, FPaths::MakeProjectRelative(PackagePath));
    HashString(Hash, CompatibilitySignature);
    return FString("SKEL-") + Hex64(Hash);
}

bool FSkeletonManager::AreSkeletonsSameStructure(const FReferenceSkeleton& A, const FReferenceSkeleton& B, FSkeletonCompatibilityReport* OutReport)
{
    bool        bCompatible = true;
    const int32 NumBonesA   = A.GetNumBones();
    const int32 NumBonesB   = B.GetNumBones();

    if (NumBonesA != NumBonesB)
    {
        bCompatible = false;
        if (OutReport)
        {
            OutReport->Reason = "bone count mismatch";
        }
    }

    const int32 MaxBones = (NumBonesA > NumBonesB) ? NumBonesA : NumBonesB;
    for (int32 BoneIndex = 0; BoneIndex < MaxBones; ++BoneIndex)
    {
        if (BoneIndex >= NumBonesA)
        {
            bCompatible = false;
            if (OutReport)
            {
                OutReport->ExtraBones.push_back(B.Bones[BoneIndex].Name);
            }
            continue;
        }

        if (BoneIndex >= NumBonesB)
        {
            bCompatible = false;
            if (OutReport)
            {
                OutReport->MissingBones.push_back(A.Bones[BoneIndex].Name);
            }
            continue;
        }

        const FReferenceBone& BoneA = A.Bones[BoneIndex];
        const FReferenceBone& BoneB = B.Bones[BoneIndex];

        if (BoneA.Name != BoneB.Name)
        {
            bCompatible = false;
            if (OutReport)
            {
                OutReport->MissingBones.push_back(BoneA.Name);
                OutReport->ExtraBones.push_back(BoneB.Name);
            }
        }

        if (BoneA.ParentIndex != BoneB.ParentIndex)
        {
            bCompatible = false;
            if (OutReport)
            {
                OutReport->ParentMismatchBones.push_back(BoneA.Name);
            }
        }
    }

    if (OutReport && bCompatible)
    {
        OutReport->Result = ESkeletonCompatibilityResult::SameStructure;
        OutReport->Reason = "same bone structure";
    }
    else if (OutReport && OutReport->Reason.empty())
    {
        OutReport->Reason = "bone hierarchy mismatch";
    }

    return bCompatible;
}

bool FSkeletonManager::BuildBoneRemapByName(
    const FReferenceSkeleton&     Source,
    const FReferenceSkeleton&     Target,
    FSkeletonBoneRemap&           OutRemap,
    FSkeletonCompatibilityReport* OutReport,
    bool                          bRequireExactBoneSet
    )
{
    OutRemap.Reset();

    const int32 SourceBoneCount = Source.GetNumBones();
    const int32 TargetBoneCount = Target.GetNumBones();

    OutRemap.SourceToTargetBone.resize(SourceBoneCount, -1);
    OutRemap.TargetToSourceBone.resize(TargetBoneCount, -1);

    TMap<FString, int32> TargetNameToIndex;
    for (int32 TargetIndex = 0; TargetIndex < TargetBoneCount; ++TargetIndex)
    {
        const FString& TargetName = Target.Bones[TargetIndex].Name;
        if (TargetNameToIndex.find(TargetName) != TargetNameToIndex.end())
        {
            if (OutReport)
            {
                OutReport->Result = ESkeletonCompatibilityResult::Incompatible;
                OutReport->Reason = "duplicate bone name in target skeleton";
                OutReport->ExtraBones.push_back(TargetName);
            }
            return false;
        }
        TargetNameToIndex[TargetName] = TargetIndex;
    }

    for (int32 SourceIndex = 0; SourceIndex < SourceBoneCount; ++SourceIndex)
    {
        const FReferenceBone& SourceBone = Source.Bones[SourceIndex];

        auto TargetIt = TargetNameToIndex.find(SourceBone.Name);
        if (TargetIt == TargetNameToIndex.end())
        {
            if (OutReport)
            {
                OutReport->Result = ESkeletonCompatibilityResult::Incompatible;
                OutReport->Reason = "target skeleton is missing source bone";
                OutReport->MissingBones.push_back(SourceBone.Name);
            }
            return false;
        }

        const int32           TargetIndex = TargetIt->second;
        const FReferenceBone& TargetBone  = Target.Bones[TargetIndex];

        const FString SourceParentName = SourceBone.ParentIndex >= 0 ? Source.Bones[SourceBone.ParentIndex].Name : FString();

        const FString TargetParentName = TargetBone.ParentIndex >= 0 ? Target.Bones[TargetBone.ParentIndex].Name : FString();

        if (SourceParentName != TargetParentName)
        {
            if (OutReport)
            {
                OutReport->Result = ESkeletonCompatibilityResult::Incompatible;
                OutReport->Reason = "bone parent mismatch";
                OutReport->ParentMismatchBones.push_back(SourceBone.Name);
            }
            return false;
        }

        OutRemap.SourceToTargetBone[SourceIndex] = TargetIndex;
        OutRemap.TargetToSourceBone[TargetIndex] = SourceIndex;
    }

    if (bRequireExactBoneSet)
    {
        if (SourceBoneCount != TargetBoneCount)
        {
            if (OutReport)
            {
                OutReport->Result = ESkeletonCompatibilityResult::Incompatible;
                OutReport->Reason = "bone count mismatch";
            }
            return false;
        }

        for (int32 TargetIndex = 0; TargetIndex < TargetBoneCount; ++TargetIndex)
        {
            if (OutRemap.TargetToSourceBone[TargetIndex] < 0)
            {
                if (OutReport)
                {
                    OutReport->Result = ESkeletonCompatibilityResult::Incompatible;
                    OutReport->Reason = "source skeleton is missing target bone";
                    OutReport->ExtraBones.push_back(Target.Bones[TargetIndex].Name);
                }
                return false;
            }
        }
    }

    if (OutReport)
    {
        OutReport->Result = ESkeletonCompatibilityResult::SameStructure;
        OutReport->Reason = "bone remap built by name";
    }
    return true;
}

void FSkeletonManager::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (auto& [Path, Skeleton] : SkeletonCaches)
	{
		Collector.AddReferencedObject(Skeleton);
	}
}

FSkeletonCompatibilityReport FSkeletonManager::CheckCompatibility(
    const FSkeletonBinding& A,
    const FSkeletonBinding& B,
    const USkeleton*        LoadedA,
    const USkeleton*        LoadedB
    )
{
    FSkeletonCompatibilityReport Report;

    if (A.HasAssetGuid() && B.HasAssetGuid() && A.SkeletonAssetGuid == B.SkeletonAssetGuid)
    {
        Report.Result = ESkeletonCompatibilityResult::ExactSkeleton;
        Report.Reason = "same skeleton asset guid";
        return Report;
    }

    if (A.HasCompatibilitySignature() && B.HasCompatibilitySignature() && A.CompatibilitySignature == B.CompatibilitySignature)
    {
        Report.Result = ESkeletonCompatibilityResult::SameStructure;
        Report.Reason = "same compatibility signature";
        return Report;
    }

    if (LoadedA && LoadedB && AreSkeletonsSameStructure(LoadedA->GetReferenceSkeleton(), LoadedB->GetReferenceSkeleton(), &Report))
    {
        Report.Result = ESkeletonCompatibilityResult::SameStructure;
        Report.Reason = "same reference skeleton structure";
        return Report;
    }

    if (Report.Reason.empty())
    {
        Report.Reason = "skeleton binding mismatch";
    }
    Report.Result = ESkeletonCompatibilityResult::Incompatible;
    return Report;
}

void FSkeletonManager::ScanSkeletonAssets()
{
    AvailableSkeletonFiles.clear();

    namespace fs = std::filesystem;
    const fs::path ContentDir = fs::path(FPaths::RootDir()) / L"Content";
    if (!fs::exists(ContentDir) || !fs::is_directory(ContentDir))
    {
        return;
    }

    for (const auto& Entry : fs::recursive_directory_iterator(ContentDir))
    {
        if (!Entry.is_regular_file())
        {
            continue;
        }

        if (Entry.path().extension() != L".uasset")
        {
            continue;
        }

        const FString RelPath = FPaths::MakeProjectRelative(FPaths::ToUtf8(Entry.path().generic_wstring()));

        EAssetPackageType Type = EAssetPackageType::Unknown;
        if (!FAssetPackage::GetPackageType(RelPath, Type) || Type != EAssetPackageType::Skeleton)
        {
            continue;
        }

        FAssetListItem Item;
        Item.DisplayName = GetDisplayNameFromPath(Entry.path());
        Item.FullPath    = RelPath;
        AvailableSkeletonFiles.push_back(Item);
    }
}

const TArray<FAssetListItem>& FSkeletonManager::GetAvailableSkeletonFiles()
{
    if (AvailableSkeletonFiles.empty())
    {
        ScanSkeletonAssets();
    }
    return AvailableSkeletonFiles;
}

USkeleton* FSkeletonManager::FindSkeletonByAssetGuid(const FString& SkeletonAssetGuid)
{
    if (SkeletonAssetGuid.empty())
    {
        return nullptr;
    }

    for (const FAssetListItem& Item : GetAvailableSkeletonFiles())
    {
        USkeleton* Skeleton = LoadSkeleton(Item.FullPath);
        if (Skeleton && Skeleton->GetSkeletonAssetGuid() == SkeletonAssetGuid)
        {
            return Skeleton;
        }
    }

    return nullptr;
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

    if (Skeleton->GetCompatibilitySignature().empty())
    {
        Skeleton->SetCompatibilitySignature(BuildCompatibilitySignature(Skeleton->GetReferenceSkeleton()));
    }
    if (Skeleton->GetSkeletonAssetGuid().empty())
    {
        Skeleton->SetSkeletonAssetGuid(MakeSkeletonAssetGuid(NormalizedPath, Skeleton->GetCompatibilitySignature()));
    }
    Skeleton->RebuildBoneNameCache();

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

    if (Skeleton->GetCompatibilitySignature().empty())
    {
        Skeleton->SetCompatibilitySignature(BuildCompatibilitySignature(Skeleton->GetReferenceSkeleton()));
    }
    if (Skeleton->GetSkeletonAssetGuid().empty())
    {
        Skeleton->SetSkeletonAssetGuid(MakeSkeletonAssetGuid(NormalizedPath, Skeleton->GetCompatibilitySignature()));
    }
    Skeleton->RebuildBoneNameCache();

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

    auto ListIt = std::find_if(
        AvailableSkeletonFiles.begin(),
        AvailableSkeletonFiles.end(),
        [&](const FAssetListItem& Item)
        {
            return Item.FullPath == NormalizedPath;
        }
    );

    if (ListIt == AvailableSkeletonFiles.end())
    {
        FAssetListItem Item;
        Item.DisplayName = Skeleton->GetName();
        if (Item.DisplayName.empty())
        {
            Item.DisplayName = NormalizedPath;
        }
        Item.FullPath = NormalizedPath;
        AvailableSkeletonFiles.push_back(Item);
    }

    return true;
}
