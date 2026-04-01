#pragma once
#include "Asset/AssetManager.h"

struct FAssetLoadParams;

class ENGINE_API IAssetLoader
{
public:
    virtual ~IAssetLoader() = default;

    virtual bool       CanLoad(const FWString& Path, const FAssetLoadParams& Params) const = 0;
    virtual EAssetType GetAssetType() const = 0;

    virtual uint64  MakeBuildSignature(const FAssetLoadParams& Params) const;
    virtual UAsset* LoadAsset(const FSourceRecord& Source, const FAssetLoadParams& Params) = 0;

    // 문자열 유틸리티
    FString WidePathToUtf8(const FWString& Path) const;

protected:
    static FWString NarrowPathToWidePath(const FString& NarrowPath);
    static bool     IsAbsoluteWidePath(const FWString& Path);
    static void     NormalizePathSeparators(FWString& Path);
    static bool     HasExtension(const FWString& Path,
                                 std::initializer_list<const wchar_t*> Extensions);
};
