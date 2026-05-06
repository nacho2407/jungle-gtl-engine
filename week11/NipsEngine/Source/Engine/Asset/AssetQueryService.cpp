#include "Asset/AssetQueryService.h"

#include "Core/Paths.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <initializer_list>

namespace
{
    bool ResolveSafeAssetPath(const FString& RelativePath, std::filesystem::path& OutPath)
    {
        if (RelativePath.empty())
        {
            return false;
        }

        std::filesystem::path RawPath(FPaths::ToWide(RelativePath));
        if (RawPath.is_absolute())
        {
            return false;
        }

        std::filesystem::path CleanRelative;
        bool bSkippedLeadingAsset = false;
        for (const std::filesystem::path& Part : RawPath.lexically_normal())
        {
            const std::wstring PartString = Part.wstring();
            if (PartString.empty() || PartString == L".")
            {
                continue;
            }
            if (PartString == L"..")
            {
                return false;
            }
            if (!bSkippedLeadingAsset && CleanRelative.empty() && (PartString == L"Asset" || PartString == L"asset"))
            {
                bSkippedLeadingAsset = true;
                continue;
            }

            CleanRelative /= Part;
        }

        if (CleanRelative.empty())
        {
            return false;
        }

        OutPath = (std::filesystem::path(FPaths::RootDir()) / L"Asset" / CleanRelative).lexically_normal();
        return true;
    }

    FString ToAssetRelativePath(const std::filesystem::path& AbsolutePath)
    {
        std::error_code Ec;
        std::filesystem::path Relative = std::filesystem::relative(AbsolutePath, std::filesystem::path(FPaths::RootDir()), Ec);
        if (Ec)
        {
            Relative = AbsolutePath.lexically_normal();
        }
        return FPaths::ToUtf8(Relative.generic_wstring());
    }

    FString LowerExtension(const std::filesystem::path& Path)
    {
        FString Extension = FPaths::ToUtf8(Path.extension().wstring());
        std::transform(
            Extension.begin(),
            Extension.end(),
            Extension.begin(),
            [](unsigned char Ch)
            {
                return static_cast<char>(std::tolower(Ch));
            });
        return Extension;
    }

    bool ExtensionMatches(const FString& Extension, std::initializer_list<const char*> Candidates)
    {
        for (const char* Candidate : Candidates)
        {
            if (Extension == Candidate)
            {
                return true;
            }
        }
        return false;
    }

    bool EndsWith(const FString& Value, const FString& Suffix)
    {
        return Value.size() >= Suffix.size() &&
            Value.compare(Value.size() - Suffix.size(), Suffix.size(), Suffix) == 0;
    }

    bool IsCurveAssetFile(const std::filesystem::path& Path)
    {
        FString FileName = FPaths::ToUtf8(Path.filename().wstring());
        std::transform(
            FileName.begin(),
            FileName.end(),
            FileName.begin(),
            [](unsigned char Ch)
            {
                return static_cast<char>(std::tolower(Ch));
            });

        return EndsWith(FileName, ".curve") || EndsWith(FileName, ".curve.json");
    }

    TArray<FString> ListAssetFiles(const std::filesystem::path& SubDirectory, std::initializer_list<const char*> Extensions)
    {
        TArray<FString> Result;

        const std::filesystem::path Root = (std::filesystem::path(FPaths::RootDir()) / L"Asset" / SubDirectory).lexically_normal();
        if (!std::filesystem::exists(Root))
        {
            return Result;
        }

        std::error_code Ec;
        for (const std::filesystem::directory_entry& Entry : std::filesystem::recursive_directory_iterator(Root, Ec))
        {
            if (Ec)
            {
                break;
            }
            if (!Entry.is_regular_file())
            {
                continue;
            }

            const FString Extension = LowerExtension(Entry.path());
            if (ExtensionMatches(Extension, Extensions))
            {
                Result.push_back(ToAssetRelativePath(Entry.path()));
            }
        }

        return Result;
    }

    TArray<FString> ListCurveAssetFiles()
    {
        TArray<FString> Result;

        const std::filesystem::path Root = (std::filesystem::path(FPaths::RootDir()) / L"Asset").lexically_normal();
        if (!std::filesystem::exists(Root))
        {
            return Result;
        }

        std::error_code Ec;
        for (const std::filesystem::directory_entry& Entry : std::filesystem::recursive_directory_iterator(Root, Ec))
        {
            if (Ec)
            {
                break;
            }
            if (Entry.is_regular_file() && IsCurveAssetFile(Entry.path()))
            {
                Result.push_back(ToAssetRelativePath(Entry.path()));
            }
        }

        return Result;
    }
}

bool FAssetQueryService::NormalizeAssetPath(const FString& Path, FString& OutRelativePath)
{
    std::filesystem::path AssetPath;
    if (!ResolveSafeAssetPath(Path, AssetPath))
    {
        OutRelativePath.clear();
        return false;
    }

    OutRelativePath = ToAssetRelativePath(AssetPath);
    return true;
}

bool FAssetQueryService::Exists(const FString& Path)
{
    std::filesystem::path AssetPath;
    return ResolveSafeAssetPath(Path, AssetPath) && std::filesystem::exists(AssetPath);
}

TArray<FString> FAssetQueryService::GetTexturePaths()
{
    return ListAssetFiles(L"Texture", { ".png", ".jpg", ".jpeg", ".dds", ".bmp", ".tga" });
}

TArray<FString> FAssetQueryService::GetStaticMeshPaths()
{
    return ListAssetFiles(L"Mesh", { ".obj", ".bin" });
}

TArray<FString> FAssetQueryService::GetMaterialPaths()
{
    return ListAssetFiles(L"Material", { ".mat", ".matinst" });
}

TArray<FString> FAssetQueryService::GetCurvePaths()
{
    return ListCurveAssetFiles();
}

TArray<FString> FAssetQueryService::GetScenePaths()
{
    return ListAssetFiles(L"Scene", { ".scene" });
}

TArray<FString> FAssetQueryService::GetSoundPaths()
{
    return ListAssetFiles(L"Sound", { ".wav", ".ogg", ".mp3" });
}
