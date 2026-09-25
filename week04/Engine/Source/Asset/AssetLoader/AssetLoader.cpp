#include "Core/CoreMinimal.h"
#include "AssetLoader.h"

#include <filesystem>
#include <algorithm>
#include <cwctype>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

FString IAssetLoader::WidePathToUtf8(const FWString& Path) const
{
    const std::filesystem::path FilePath(Path);
    const std::u8string         Utf8Path = FilePath.u8string();
    return FString(reinterpret_cast<const char*>(Utf8Path.data()), Utf8Path.size());
}

FWString IAssetLoader::NarrowPathToWidePath(const FString& NarrowPath)
{
    if (NarrowPath.empty())
    {
        return {};
    }

    auto ConvertWithCodePage = [&](UINT CodePage, DWORD Flags) -> FWString
    {
        const int32 RequiredChars =
            MultiByteToWideChar(CodePage, Flags, NarrowPath.data(),
                                static_cast<int>(NarrowPath.size()), nullptr, 0);
        if (RequiredChars <= 0)
        {
            return {};
        }

        FWString WidePath(static_cast<size_t>(RequiredChars), L'\0');
        const int32 ConvertedChars =
            MultiByteToWideChar(CodePage, Flags, NarrowPath.data(),
                                static_cast<int>(NarrowPath.size()), WidePath.data(),
                                RequiredChars);
        if (ConvertedChars <= 0)
        {
            return {};
        }

        return WidePath;
    };

    FWString WidePath = ConvertWithCodePage(CP_UTF8, MB_ERR_INVALID_CHARS);
    if (!WidePath.empty())
    {
        return WidePath;
    }

    WidePath = ConvertWithCodePage(CP_ACP, 0);
    if (!WidePath.empty())
    {
        return WidePath;
    }

    return ConvertWithCodePage(CP_OEMCP, 0);
}

bool IAssetLoader::IsAbsoluteWidePath(const FWString& Path)
{
    if (Path.size() >= 2 && Path[1] == L':')
    {
        return true;
    }

    return Path.size() >= 2 && Path[0] == L'\\' && Path[1] == L'\\';
}

void IAssetLoader::NormalizePathSeparators(FWString& Path)
{
    std::replace(Path.begin(), Path.end(), L'/', L'\\');
}

bool IAssetLoader::HasExtension(const FWString& Path,
                                std::initializer_list<const wchar_t*> Extensions)
{
    if (Path.empty())
    {
        return false;
    }

    const std::filesystem::path FilePath(Path);
    FWString                    Extension = FilePath.extension().native();
    std::transform(Extension.begin(), Extension.end(), Extension.begin(),
                   [](wchar_t Ch) { return static_cast<wchar_t>(std::towlower(Ch)); });

    for (const auto* Ext : Extensions)
    {
        if (Ext && Extension == Ext)
        {
            return true;
        }
    }
    return false;
}

uint64 IAssetLoader::MakeBuildSignature(const FAssetLoadParams& Params) const
{
    (void)Params;
    uint64 Hash = 14695981039346656037ull;
    Hash ^= static_cast<uint64>(GetAssetType());
    Hash *= 1099511628211ull;
    return Hash;
}
