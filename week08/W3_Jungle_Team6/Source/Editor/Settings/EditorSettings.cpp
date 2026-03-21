#include "Editor/Settings/EditorSettings.h"
#include "SimpleJSON/json.hpp"

#include <fstream>
#include <filesystem>

void FEditorSettings::SaveToFile(const FString& Path) const
{
	using namespace json;

	JSON Root = Object();

	// Viewport
	JSON Viewport = Object();
	Viewport["CameraSpeed"] = CameraSpeed;
	Viewport["CameraRotationSpeed"] = CameraRotationSpeed;
	Viewport["CameraZoomSpeed"] = CameraZoomSpeed;

	JSON InitPos = Array(InitViewPos.X, InitViewPos.Y, InitViewPos.Z);
	Viewport["InitViewPos"] = InitPos;

	JSON LookAt = Array(InitLookAt.X, InitLookAt.Y, InitLookAt.Z);
	Viewport["InitLookAt"] = LookAt;

	Root["Viewport"] = Viewport;

	// Runtime
	JSON Runtime = Object();
	Runtime["bLimitUpdateRate"] = bLimitUpdateRate;
	Runtime["UpdateRate"] = UpdateRate;
	Root["Runtime"] = Runtime;

	// Paths
	JSON Paths = Object();
	Paths["DefaultSavePath"] = DefaultSavePath;
	Root["Paths"] = Paths;

	// Ensure directory exists
	std::filesystem::path FilePath(FPaths::ToWide(Path));
	if (FilePath.has_parent_path())
	{
		std::filesystem::create_directories(FilePath.parent_path());
	}

	std::ofstream File(FilePath);
	if (File.is_open())
	{
		File << Root;
	}
}

void FEditorSettings::LoadFromFile(const FString& Path)
{
	using namespace json;

	std::ifstream File(std::filesystem::path(FPaths::ToWide(Path)));
	if (!File.is_open())
	{
		return;
	}

	FString Content((std::istreambuf_iterator<char>(File)),
		std::istreambuf_iterator<char>());

	JSON Root = JSON::Load(Content);

	// Viewport
	if (Root.hasKey("Viewport"))
	{
		JSON Viewport = Root["Viewport"];

		if (Viewport.hasKey("CameraSpeed"))
			CameraSpeed = static_cast<float>(Viewport["CameraSpeed"].ToFloat());
		if (Viewport.hasKey("CameraRotationSpeed"))
			CameraRotationSpeed = static_cast<float>(Viewport["CameraRotationSpeed"].ToFloat());
		if (Viewport.hasKey("CameraZoomSpeed"))
			CameraZoomSpeed = static_cast<float>(Viewport["CameraZoomSpeed"].ToFloat());

		if (Viewport.hasKey("InitViewPos"))
		{
			JSON Pos = Viewport["InitViewPos"];
			InitViewPos = FVector(
				static_cast<float>(Pos[0].ToFloat()),
				static_cast<float>(Pos[1].ToFloat()),
				static_cast<float>(Pos[2].ToFloat()));
		}

		if (Viewport.hasKey("InitLookAt"))
		{
			JSON Look = Viewport["InitLookAt"];
			InitLookAt = FVector(
				static_cast<float>(Look[0].ToFloat()),
				static_cast<float>(Look[1].ToFloat()),
				static_cast<float>(Look[2].ToFloat()));
		}
	}

	// Runtime
	if (Root.hasKey("Runtime"))
	{
		JSON Runtime = Root["Runtime"];

		if (Runtime.hasKey("bLimitUpdateRate"))
			bLimitUpdateRate = Runtime["bLimitUpdateRate"].ToBool();
		if (Runtime.hasKey("UpdateRate"))
			UpdateRate = Runtime["UpdateRate"].ToInt();
	}

	// Paths
	if (Root.hasKey("Paths"))
	{
		JSON PathsObj = Root["Paths"];

		if (PathsObj.hasKey("DefaultSavePath"))
			DefaultSavePath = PathsObj["DefaultSavePath"].ToString();
	}
}
