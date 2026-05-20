#include "Asset/AnimSequenceAssetLoader.h"

#include "Core/Logging/Log.h"
#include "Core/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "SimpleJSON/json.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>

namespace
{
    FString NormalizeAnimSequenceAssetPath(const FString& Path)
    {
        return FPaths::Normalize(Path);
    }

    bool IsAnimSequenceAssetPath(const FString& Path)
    {
        FString LowerPath = FPaths::Normalize(Path);
        std::transform(
            LowerPath.begin(),
            LowerPath.end(),
            LowerPath.begin(),
            [](unsigned char Ch)
            {
                return static_cast<char>(std::tolower(Ch));
            });

        return std::filesystem::path(FPaths::ToWide(LowerPath)).extension() == L".animsequence";
    }
}

bool FAnimSequenceAssetDescriptor::IsValid() const
{
    return !SourceFbxPath.empty() && !TargetSkeletalMeshPath.empty() && !AnimStackName.empty();
}

void FAnimSequenceAssetDescriptor::Serialize(FArchive& Ar)
{
    Ar << "AssetPath" << AssetPath;
    Ar << "SourceFbxPath" << SourceFbxPath;
    Ar << "TargetSkeletalMeshPath" << TargetSkeletalMeshPath;
    Ar << "AnimStackName" << AnimStackName;
}

bool FAnimSequenceAssetLoader::Load(const FString& Path, FAnimSequenceAssetDescriptor& OutDescriptor) const
{
    const FString NormalizedPath = NormalizeAnimSequenceAssetPath(Path);
    if (NormalizedPath.empty() || !IsAnimSequenceAssetPath(NormalizedPath))
    {
        return false;
    }

    std::ifstream AssetFile(std::filesystem::path(FPaths::ToAbsolute(FPaths::ToWide(NormalizedPath))));
    if (!AssetFile.is_open())
    {
        UE_LOG_ERROR("[AnimSequenceAssetLoader] Failed to open anim sequence asset: %s", NormalizedPath.c_str());
        return false;
    }

    FString FileContent((std::istreambuf_iterator<char>(AssetFile)), std::istreambuf_iterator<char>());
    json::JSON Root = json::JSON::Load(FileContent);
    if (Root.JSONType() != json::JSON::Class::Object)
    {
        UE_LOG_ERROR("[AnimSequenceAssetLoader] Invalid anim sequence asset json: %s", NormalizedPath.c_str());
        return false;
    }

    FJsonReader Reader(Root);
    OutDescriptor = {};
    OutDescriptor.Serialize(Reader);
    if (OutDescriptor.AssetPath.empty())
    {
        OutDescriptor.AssetPath = NormalizedPath;
    }

    if (!OutDescriptor.IsValid())
    {
        UE_LOG_ERROR("[AnimSequenceAssetLoader] Invalid anim sequence asset descriptor: %s", NormalizedPath.c_str());
        return false;
    }

    return true;
}

bool FAnimSequenceAssetLoader::Save(const FString& Path, const FAnimSequenceAssetDescriptor& Descriptor) const
{
    const FString NormalizedPath = NormalizeAnimSequenceAssetPath(Path);
    if (NormalizedPath.empty() || !IsAnimSequenceAssetPath(NormalizedPath) || !Descriptor.IsValid())
    {
        return false;
    }

    FAnimSequenceAssetDescriptor WritableDescriptor = Descriptor;
    WritableDescriptor.AssetPath = NormalizedPath;

    json::JSON Root = json::JSON::Make(json::JSON::Class::Object);
    FJsonWriter Writer(Root);
    WritableDescriptor.Serialize(Writer);

    std::error_code ErrorCode;
    std::filesystem::path FilePath(FPaths::ToAbsolute(FPaths::ToWide(NormalizedPath)));
    std::filesystem::create_directories(FilePath.parent_path(), ErrorCode);
    if (ErrorCode)
    {
        UE_LOG_ERROR("[AnimSequenceAssetLoader] Failed to create anim sequence asset directory: %s", NormalizedPath.c_str());
        return false;
    }

    std::ofstream OutFile(FilePath);
    if (!OutFile.is_open())
    {
        UE_LOG_ERROR("[AnimSequenceAssetLoader] Failed to open anim sequence asset for writing: %s", NormalizedPath.c_str());
        return false;
    }

    OutFile << Root.dump(4);
    return true;
}

bool FAnimSequenceAssetLoader::SupportsExtension(const FString& Extension) const
{
    return Extension == ".animsequence" || Extension == "animsequence";
}

FString FAnimSequenceAssetLoader::GetLoaderName() const
{
    return "FAnimSequenceAssetLoader";
}
