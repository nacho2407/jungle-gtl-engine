#pragma once

#include "Core/CoreTypes.h"

#include <filesystem>
#include <string>

constexpr const char* TextureAtlasAssetKey_Image = "Image";
constexpr const char* TextureAtlasAssetKey_Columns = "Columns";
constexpr const char* TextureAtlasAssetKey_Rows = "Rows";

enum class ETextureAtlasAssetType
{
	Font,
	SubUV
};

struct FTextureAtlasAsset
{
	ETextureAtlasAssetType Type = ETextureAtlasAssetType::SubUV;
	FString ImagePath;
	int32 Columns = 1;
	int32 Rows = 1;
};

class FTextureAtlasAssetService
{
public:
	static bool Load(
		const std::filesystem::path& AssetFilePath,
		ETextureAtlasAssetType Type,
		const std::filesystem::path& ProjectRootPath,
		FTextureAtlasAsset& OutAsset);
};
