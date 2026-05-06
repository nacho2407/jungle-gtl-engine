#include "Asset/CurveAssetLoader.h"

#include "Asset/CurveFloatAsset.h"
#include "Core/Logging/Log.h"
#include "Core/Paths.h"
#include "Object/Object.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "SimpleJSON/json.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>

namespace
{
    FString NormalizeCurvePath(const FString& Path)
    {
        return FPaths::Normalize(Path);
    }

    bool IsCurveAssetPath(const FString& Path)
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

        const FString CurveJsonSuffix = ".curve.json";
        if (LowerPath.size() >= CurveJsonSuffix.size() &&
            LowerPath.compare(LowerPath.size() - CurveJsonSuffix.size(), CurveJsonSuffix.size(), CurveJsonSuffix) == 0)
        {
            return true;
        }

        return std::filesystem::path(FPaths::ToWide(LowerPath)).extension() == L".curve";
    }
}

UCurveFloatAsset* FCurveAssetLoader::Load(const FString& Path) const
{
    const FString NormalizedPath = NormalizeCurvePath(Path);
    if (NormalizedPath.empty() || !IsCurveAssetPath(NormalizedPath))
    {
        return nullptr;
    }

    std::ifstream CurveFile(FPaths::ToWide(NormalizedPath));
    if (!CurveFile.is_open())
    {
        UE_LOG_ERROR("[CurveAssetLoader] Failed to open curve asset: %s", NormalizedPath.c_str());
        return nullptr;
    }

    FString FileContent((std::istreambuf_iterator<char>(CurveFile)), std::istreambuf_iterator<char>());
    json::JSON Root = json::JSON::Load(FileContent);
    if (Root.JSONType() != json::JSON::Class::Object)
    {
        UE_LOG_ERROR("[CurveAssetLoader] Invalid curve asset json: %s", NormalizedPath.c_str());
        return nullptr;
    }

    UCurveFloatAsset* Curve = UObjectManager::Get().CreateObject<UCurveFloatAsset>();
    FJsonReader Reader(Root);
    Curve->Serialize(Reader);
    Curve->SetAssetPath(NormalizedPath);
    Curve->GetMutableCurve().SortKeys();
    return Curve;
}

bool FCurveAssetLoader::Save(const FString& Path, const UCurveFloatAsset* Curve) const
{
    if (!Curve)
    {
        return false;
    }

    const FString NormalizedPath = NormalizeCurvePath(Path);
    if (NormalizedPath.empty() || !IsCurveAssetPath(NormalizedPath))
    {
        return false;
    }

    json::JSON Root = json::JSON::Make(json::JSON::Class::Object);
    FJsonWriter Writer(Root);
    const_cast<UCurveFloatAsset*>(Curve)->Serialize(Writer);

    std::error_code ErrorCode;
    std::filesystem::path FilePath(FPaths::ToWide(NormalizedPath));
    std::filesystem::create_directories(FilePath.parent_path(), ErrorCode);

    std::ofstream OutFile(FilePath);
    if (!OutFile.is_open())
    {
        UE_LOG_ERROR("[CurveAssetLoader] Failed to open curve asset for writing: %s", NormalizedPath.c_str());
        return false;
    }

    OutFile << Root.dump(4);
    return true;
}

bool FCurveAssetLoader::SupportsExtension(const FString& Extension) const
{
    return Extension == ".curve" || Extension == "curve" ||
        Extension == ".curve.json" || Extension == "curve.json";
}

FString FCurveAssetLoader::GetLoaderName() const
{
    return "FCurveAssetLoader";
}
