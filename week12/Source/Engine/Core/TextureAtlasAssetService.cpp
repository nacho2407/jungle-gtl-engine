#include "Core/TextureAtlasAssetService.h"

#include "Core/Paths.h"
#include "SimpleJSON/json.hpp"

#include <algorithm>
#include <cwctype>
#include <fstream>
#include <iterator>

namespace
{
	namespace fs = std::filesystem;
	using namespace json;

	constexpr const wchar_t* ImageExtensions[] =
	{
		L".dds",
		L".png",
		L".jpg",
		L".jpeg",
		L".bmp",
		L".tga"
	};

	FString ToProjectRelativePath(const fs::path& AbsolutePath, const fs::path& ProjectRootPath)
	{
		std::error_code ErrorCode;
		fs::path RelativePath = fs::relative(AbsolutePath.lexically_normal(), ProjectRootPath.lexically_normal(), ErrorCode);
		if (ErrorCode || RelativePath.empty())
		{
			RelativePath = AbsolutePath.lexically_normal();
		}
		return FPaths::Normalize(FPaths::ToUtf8(RelativePath.generic_wstring()));
	}

	fs::path ResolveImagePath(
		const fs::path& AssetFilePath,
		const fs::path& ProjectRootPath,
		const FString& ImagePath)
	{
		if (ImagePath.empty())
		{
			return {};
		}

		fs::path Candidate(FPaths::ToWide(ImagePath));
		if (Candidate.is_absolute())
		{
			return Candidate.lexically_normal();
		}

		const fs::path ProjectRelative = (ProjectRootPath / Candidate).lexically_normal();
		if (fs::exists(ProjectRelative))
		{
			return ProjectRelative;
		}

		return (AssetFilePath.parent_path() / Candidate).lexically_normal();
	}

	fs::path InferSameStemImagePath(const fs::path& AssetFilePath)
	{
		const fs::path Directory = AssetFilePath.parent_path();
		const std::wstring Stem = AssetFilePath.stem().wstring();

		for (const wchar_t* Extension : ImageExtensions)
		{
			fs::path Candidate = Directory / (Stem + Extension);
			if (fs::exists(Candidate))
			{
				return Candidate.lexically_normal();
			}
		}

		std::error_code ErrorCode;
		for (const fs::directory_entry& Entry : fs::directory_iterator(Directory, ErrorCode))
		{
			if (ErrorCode || !Entry.is_regular_file())
			{
				continue;
			}

			std::wstring EntryStem = Entry.path().stem().wstring();
			std::wstring EntryExtension = Entry.path().extension().wstring();
			std::transform(EntryStem.begin(), EntryStem.end(), EntryStem.begin(), ::towlower);
			std::transform(EntryExtension.begin(), EntryExtension.end(), EntryExtension.begin(), ::towlower);

			std::wstring LowerStem = Stem;
			std::transform(LowerStem.begin(), LowerStem.end(), LowerStem.begin(), ::towlower);
			if (EntryStem != LowerStem)
			{
				continue;
			}

			for (const wchar_t* Extension : ImageExtensions)
			{
				if (EntryExtension == Extension)
				{
					return Entry.path().lexically_normal();
				}
			}
		}

		return {};
	}
}

bool FTextureAtlasAssetService::Load(
	const std::filesystem::path& AssetFilePath,
	ETextureAtlasAssetType Type,
	const std::filesystem::path& ProjectRootPath,
	FTextureAtlasAsset& OutAsset)
{
	if (!fs::exists(AssetFilePath))
	{
		return false;
	}

	std::ifstream AssetFile(AssetFilePath);
	if (!AssetFile.is_open())
	{
		return false;
	}

	std::string Content(
		(std::istreambuf_iterator<char>(AssetFile)),
		std::istreambuf_iterator<char>());

	JSON Root = JSON::Load(Content);
	if (Root.JSONType() != JSON::Class::Object)
	{
		return false;
	}

	OutAsset = {};
	OutAsset.Type = Type;
	OutAsset.Columns = 1;
	OutAsset.Rows = 1;

	if (Root.hasKey(TextureAtlasAssetKey_Columns))
	{
		OutAsset.Columns = std::max(1, static_cast<int32>(Root[TextureAtlasAssetKey_Columns].ToInt()));
	}

	if (Root.hasKey(TextureAtlasAssetKey_Rows))
	{
		OutAsset.Rows = std::max(1, static_cast<int32>(Root[TextureAtlasAssetKey_Rows].ToInt()));
	}

	fs::path ImageAbsolutePath;
	if (Root.hasKey(TextureAtlasAssetKey_Image))
	{
		ImageAbsolutePath = ResolveImagePath(
			AssetFilePath,
			ProjectRootPath,
			Root[TextureAtlasAssetKey_Image].ToString());
	}
	else
	{
		ImageAbsolutePath = InferSameStemImagePath(AssetFilePath);
	}

	if (ImageAbsolutePath.empty() || !fs::exists(ImageAbsolutePath))
	{
		return false;
	}

	OutAsset.ImagePath = ToProjectRelativePath(ImageAbsolutePath, ProjectRootPath);
	return !OutAsset.ImagePath.empty();
}
