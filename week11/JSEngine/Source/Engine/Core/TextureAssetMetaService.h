#pragma once

#include "Core/CoreTypes.h"

#include <filesystem>
#include <string>

constexpr const char* TextureMetaKey_Type = "Type";
constexpr const char* TextureMetaKey_Columns = "Columns";
constexpr const char* TextureMetaKey_Rows = "Rows";

enum class EAssetMetaType
{
	None,
	Font,
	Particle,
	Texture
};

struct FTextureAssetMeta
{
	EAssetMetaType Type = EAssetMetaType::None;
	int32 Columns = 1;
	int32 Rows = 1;
};

inline const char* ToString(EAssetMetaType Type)
{
	switch (Type)
	{
	case EAssetMetaType::Font: return "Font";
	case EAssetMetaType::Particle: return "Particle";
	case EAssetMetaType::Texture: return "Texture";
	default: return "None";
	}
}

inline EAssetMetaType ToAssetMetaType(const std::string& Value)
{
	if (Value == "Font")
	{
		return EAssetMetaType::Font;
	}
	if (Value == "Particle")
	{
		return EAssetMetaType::Particle;
	}
	if (Value == "Texture")
	{
		return EAssetMetaType::Texture;
	}
	return EAssetMetaType::None;
}

class FTextureAssetMetaService
{
public:
	static FTextureAssetMeta LoadOrCreate(const std::filesystem::path& FilePath);
};
