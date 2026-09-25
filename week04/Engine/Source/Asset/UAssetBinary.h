#pragma once

#include <cstdint>

enum class EUAssetBinaryType : uint32
{
    Unknown = 0,
    StaticMesh = 1,
    Material = 2
};

struct FUAssetBinaryHeader
{
    uint32           Magic = 0;
    uint32           Version = 0;
    EUAssetBinaryType AssetType = EUAssetBinaryType::Unknown;
    uint64           SourceHash = 0;
};

constexpr uint32 kUAssetMagic = 0x54534155; // 'UAST'
constexpr uint32 kUAssetVersion = 1;
