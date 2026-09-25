#include "Core/CoreMinimal.h"
#include "Asset.h"

#include "AssetManager.h"
#include <filesystem>

void UAsset::InitializeAssetMetadata(const FSourceRecord& InSource)
{
    SourcePath = InSource.NormalizedPath;
    ImportedHash = InSource.SourceHash;

    std::filesystem::path FilePath(InSource.NormalizedPath);
    const std::u8string   Utf8Name = FilePath.filename().u8string();
    FString ExtractedName(reinterpret_cast<const char*>(Utf8Name.data()), Utf8Name.size());
    AssetName = ExtractedName;
    Name = ExtractedName.c_str();
}

REGISTER_CLASS(, UAsset)
