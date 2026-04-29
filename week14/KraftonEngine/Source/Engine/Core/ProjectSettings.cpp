#include "Core/ProjectSettings.h"
#include "SimpleJSON/json.hpp"

#include <fstream>
#include <filesystem>

namespace PSKey
{
	constexpr const char* Shadow = "Shadow";
	constexpr const char* bShadows = "bShadows";
	constexpr const char* CSMResolution = "CSMResolution";
	constexpr const char* SpotAtlasResolution = "SpotAtlasResolution";
	constexpr const char* PointAtlasResolution = "PointAtlasResolution";
	constexpr const char* MaxSpotAtlasPages = "MaxSpotAtlasPages";
	constexpr const char* MaxPointAtlasPages = "MaxPointAtlasPages";
}

void FProjectSettings::SaveToFile(const FString& Path) const
{
	using namespace json;

	JSON Root = Object();

	JSON ShadowObj = Object();
	ShadowObj[PSKey::bShadows] = Shadow.bEnabled;
	ShadowObj[PSKey::CSMResolution] = static_cast<int>(Shadow.CSMResolution);
	ShadowObj[PSKey::SpotAtlasResolution] = static_cast<int>(Shadow.SpotAtlasResolution);
	ShadowObj[PSKey::PointAtlasResolution] = static_cast<int>(Shadow.PointAtlasResolution);
	ShadowObj[PSKey::MaxSpotAtlasPages] = static_cast<int>(Shadow.MaxSpotAtlasPages);
	ShadowObj[PSKey::MaxPointAtlasPages] = static_cast<int>(Shadow.MaxPointAtlasPages);
	Root[PSKey::Shadow] = ShadowObj;

	std::filesystem::path FilePath(FPaths::ToWide(Path));
	if (FilePath.has_parent_path())
		std::filesystem::create_directories(FilePath.parent_path());

	std::ofstream File(FilePath);
	if (File.is_open())
		File << Root;
}

void FProjectSettings::LoadFromFile(const FString& Path)
{
	using namespace json;

	std::ifstream File(std::filesystem::path(FPaths::ToWide(Path)));
	if (!File.is_open())
		return;

	FString Content((std::istreambuf_iterator<char>(File)),
		std::istreambuf_iterator<char>());

	JSON Root = JSON::Load(Content);

	if (Root.hasKey(PSKey::Shadow))
	{
		JSON S = Root[PSKey::Shadow];
		if (S.hasKey(PSKey::bShadows))
			Shadow.bEnabled = S[PSKey::bShadows].ToBool();
		if (S.hasKey(PSKey::CSMResolution))
		{
			int v = S[PSKey::CSMResolution].ToInt();
			Shadow.CSMResolution = static_cast<uint32>((std::max)(64, (std::min)(v, 8192)));
		}
		if (S.hasKey(PSKey::SpotAtlasResolution))
		{
			int v = S[PSKey::SpotAtlasResolution].ToInt();
			Shadow.SpotAtlasResolution = static_cast<uint32>((std::max)(64, (std::min)(v, 8192)));
		}
		if (S.hasKey(PSKey::PointAtlasResolution))
		{
			int v = S[PSKey::PointAtlasResolution].ToInt();
			Shadow.PointAtlasResolution = static_cast<uint32>((std::max)(64, (std::min)(v, 8192)));
		}
		if (S.hasKey(PSKey::MaxSpotAtlasPages))
		{
			int v = S[PSKey::MaxSpotAtlasPages].ToInt();
			Shadow.MaxSpotAtlasPages = static_cast<uint32>(v > 1 ? v : 1);
		}
		if (S.hasKey(PSKey::MaxPointAtlasPages))
		{
			int v = S[PSKey::MaxPointAtlasPages].ToInt();
			Shadow.MaxPointAtlasPages = static_cast<uint32>(v > 1 ? v : 1);
		}
	}
}
