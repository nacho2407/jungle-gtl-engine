#include "Editor/Settings/ProjectSettings.h"
#include "SimpleJSON/json.hpp"

#include <fstream>
#include <filesystem>

namespace PSKey
{
	constexpr const char* Rendering = "Rendering";
	constexpr const char* bShadows = "bShadows";
}

void FProjectSettings::SaveToFile(const FString& Path) const
{
	using namespace json;

	JSON Root = Object();

	JSON RenderObj = Object();
	RenderObj[PSKey::bShadows] = bShadows;
	Root[PSKey::Rendering] = RenderObj;

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

	if (Root.hasKey(PSKey::Rendering))
	{
		JSON R = Root[PSKey::Rendering];
		if (R.hasKey(PSKey::bShadows))
			bShadows = R[PSKey::bShadows].ToBool();
	}
}
