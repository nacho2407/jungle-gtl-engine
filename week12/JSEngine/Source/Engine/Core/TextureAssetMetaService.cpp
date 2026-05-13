#include "Core/TextureAssetMetaService.h"

#include "SimpleJSON/json.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>

namespace
{
	namespace fs = std::filesystem;
	using namespace json;

	EAssetMetaType InferAssetMetaTypeFromParentDirectory(const fs::path& FilePath)
	{
		const std::wstring ParentDir = FilePath.parent_path().filename().wstring();
		if (ParentDir == L"Font")
		{
			return EAssetMetaType::Font;
		}
		if (ParentDir == L"Particle")
		{
			return EAssetMetaType::Particle;
		}
		if (ParentDir == L"Texture")
		{
			return EAssetMetaType::Texture;
		}
		return EAssetMetaType::None;
	}
}

FTextureAssetMeta FTextureAssetMetaService::LoadOrCreate(const std::filesystem::path& FilePath)
{
	FTextureAssetMeta Meta;

	fs::path MetaPath = FilePath;
	MetaPath.replace_extension(L".meta");

	if (fs::exists(MetaPath))
	{
		std::ifstream MetaFile(MetaPath);
		if (MetaFile.is_open())
		{
			std::string Content((std::istreambuf_iterator<char>(MetaFile)),
			                    std::istreambuf_iterator<char>());

			JSON Root = JSON::Load(Content);
			if (Root.JSONType() == JSON::Class::Object)
			{
				if (Root.hasKey(TextureMetaKey_Type))
				{
					Meta.Type = ToAssetMetaType(Root[TextureMetaKey_Type].ToString());
				}

				if (Root.hasKey(TextureMetaKey_Columns))
				{
					Meta.Columns = std::max(1, static_cast<int32>(Root[TextureMetaKey_Columns].ToInt()));
				}

				if (Root.hasKey(TextureMetaKey_Rows))
				{
					Meta.Rows = std::max(1, static_cast<int32>(Root[TextureMetaKey_Rows].ToInt()));
				}
			}
		}

		if (Meta.Type == EAssetMetaType::None && InferAssetMetaTypeFromParentDirectory(FilePath) == EAssetMetaType::Texture)
		{
			Meta.Type = EAssetMetaType::Texture;
		}

		return Meta;
	}

	Meta.Type = InferAssetMetaTypeFromParentDirectory(FilePath);
	Meta.Columns = 1;
	Meta.Rows = 1;

	JSON Root = JSON::Make(JSON::Class::Object);
	Root[TextureMetaKey_Type] = ToString(Meta.Type);
	Root[TextureMetaKey_Columns] = Meta.Columns;
	Root[TextureMetaKey_Rows] = Meta.Rows;

	std::ofstream OutFile(MetaPath);
	if (OutFile.is_open())
	{
		OutFile << Root.dump();
	}

	return Meta;
}
