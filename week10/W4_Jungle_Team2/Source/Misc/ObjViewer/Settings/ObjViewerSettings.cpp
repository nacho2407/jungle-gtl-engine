#include "Misc/ObjViewer/Settings/ObjViewerSettings.h"
#include "SimpleJSON/json.hpp"

#include <fstream>
#include <filesystem>

namespace ObjViewerKey
{
	// Section
	constexpr const char* Viewport = "Viewport";
	constexpr const char* Paths = "Paths";

	// Viewport
	constexpr const char* CameraSpeed = "CameraSpeed";
	constexpr const char* CameraRotationSpeed = "CameraRotationSpeed";
	constexpr const char* CameraForwardSpeed = "CameraForwardSpeed";
	constexpr const char* CameraMoveSensitivity = "CameraMoveSensitivity";
	constexpr const char* CameraRotateSensitivity = "CameraRotateSensitivity";
	constexpr const char* InitViewPos = "InitViewPos";
	constexpr const char* InitLookAt = "InitLookAt";

	// View
	constexpr const char* View = "View";
	constexpr const char* ViewMode = "ViewMode";
	constexpr const char* bPrimitives = "bPrimitives";
	constexpr const char* bGrid = "bGrid";
	constexpr const char* bAxis = "bAxis";

	// Grid
	constexpr const char* Grid = "Grid";
	constexpr const char* GridSpacing = "GridSpacing";
	constexpr const char* GridHalfLineCount = "GridHalfLineCount";

	// Paths
	constexpr const char* DefaultSavePath = "DefaultSavePath";
}

void FObjViewerSettings::SaveToFile(const FString& Path) const
{
	using namespace json;

	JSON Root = Object();

	// Viewport
	JSON Viewport = Object();
	Viewport[ObjViewerKey::CameraSpeed] = CameraSpeed;
	Viewport[ObjViewerKey::CameraRotationSpeed] = CameraRotationSpeed;
	Viewport[ObjViewerKey::CameraForwardSpeed] = CameraForwardSpeed;
	Viewport[ObjViewerKey::CameraMoveSensitivity] = CameraMoveSensitivity;
	Viewport[ObjViewerKey::CameraRotateSensitivity] = CameraRotateSensitivity;

	JSON InitPos = Array(InitViewPos.X, InitViewPos.Y, InitViewPos.Z);
	Viewport[ObjViewerKey::InitViewPos] = InitPos;

	JSON LookAt = Array(InitLookAt.X, InitLookAt.Y, InitLookAt.Z);
	Viewport[ObjViewerKey::InitLookAt] = LookAt;

	Root[ObjViewerKey::Viewport] = Viewport;

	// View
	JSON ViewObj = Object();
	ViewObj[ObjViewerKey::ViewMode] = static_cast<int32>(ViewMode);
	ViewObj[ObjViewerKey::bPrimitives] = ShowFlags.bPrimitives;
	ViewObj[ObjViewerKey::bGrid] = ShowFlags.bGrid;
	ViewObj[ObjViewerKey::bAxis] = ShowFlags.bAxis;
	Root[ObjViewerKey::View] = ViewObj;

	// Grid
	JSON GridObj = Object();
	GridObj[ObjViewerKey::GridSpacing] = GridSpacing;
	GridObj[ObjViewerKey::GridHalfLineCount] = GridHalfLineCount;
	Root[ObjViewerKey::Grid] = GridObj;

	// Paths
	JSON PathsObj = Object();
	PathsObj[ObjViewerKey::DefaultSavePath] = DefaultSavePath;
	Root[ObjViewerKey::Paths] = PathsObj;

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

void FObjViewerSettings::LoadFromFile(const FString& Path)
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
	if (Root.hasKey(ObjViewerKey::Viewport))
	{
		JSON Viewport = Root[ObjViewerKey::Viewport];

		if (Viewport.hasKey(ObjViewerKey::CameraSpeed))
			CameraSpeed = static_cast<float>(Viewport[ObjViewerKey::CameraSpeed].ToFloat());
		if (Viewport.hasKey(ObjViewerKey::CameraRotationSpeed))
			CameraRotationSpeed = static_cast<float>(Viewport[ObjViewerKey::CameraRotationSpeed].ToFloat());
		if (Viewport.hasKey(ObjViewerKey::CameraForwardSpeed))
			CameraForwardSpeed = static_cast<float>(Viewport[ObjViewerKey::CameraForwardSpeed].ToFloat());
		if (Viewport.hasKey(ObjViewerKey::CameraMoveSensitivity))
			CameraMoveSensitivity = static_cast<float>(Viewport[ObjViewerKey::CameraMoveSensitivity].ToFloat());
		if (Viewport.hasKey(ObjViewerKey::CameraRotateSensitivity))
			CameraRotateSensitivity = static_cast<float>(Viewport[ObjViewerKey::CameraRotateSensitivity].ToFloat());

		if (Viewport.hasKey(ObjViewerKey::InitViewPos))
		{
			JSON Pos = Viewport[ObjViewerKey::InitViewPos];
			InitViewPos = FVector(
				static_cast<float>(Pos[0].ToFloat()),
				static_cast<float>(Pos[1].ToFloat()),
				static_cast<float>(Pos[2].ToFloat()));
		}

		if (Viewport.hasKey(ObjViewerKey::InitLookAt))
		{
			JSON Look = Viewport[ObjViewerKey::InitLookAt];
			InitLookAt = FVector(
				static_cast<float>(Look[0].ToFloat()),
				static_cast<float>(Look[1].ToFloat()),
				static_cast<float>(Look[2].ToFloat()));
		}
	}

	// View
	if (Root.hasKey(ObjViewerKey::View))
	{
		JSON ViewObj = Root[ObjViewerKey::View];

		if (ViewObj.hasKey(ObjViewerKey::ViewMode))
		{
			int32 Mode = ViewObj[ObjViewerKey::ViewMode].ToInt();
			if (Mode >= 0 && Mode < static_cast<int32>(EViewMode::Count))
				ViewMode = static_cast<EViewMode>(Mode);
		}
		if (ViewObj.hasKey(ObjViewerKey::bPrimitives))
			ShowFlags.bPrimitives = ViewObj[ObjViewerKey::bPrimitives].ToBool();
		if (ViewObj.hasKey(ObjViewerKey::bGrid))
			ShowFlags.bGrid = ViewObj[ObjViewerKey::bGrid].ToBool();
		if (ViewObj.hasKey(ObjViewerKey::bAxis))
			ShowFlags.bAxis = ViewObj[ObjViewerKey::bAxis].ToBool();
	}

	// Grid
	if (Root.hasKey(ObjViewerKey::Grid))
	{
		JSON GridObj = Root[ObjViewerKey::Grid];

		if (GridObj.hasKey(ObjViewerKey::GridSpacing))
			GridSpacing = static_cast<float>(GridObj[ObjViewerKey::GridSpacing].ToFloat());
		if (GridObj.hasKey(ObjViewerKey::GridHalfLineCount))
			GridHalfLineCount = GridObj[ObjViewerKey::GridHalfLineCount].ToInt();
	}

	// Paths
	if (Root.hasKey(ObjViewerKey::Paths))
	{
		JSON PathsObj = Root[ObjViewerKey::Paths];

		if (PathsObj.hasKey(ObjViewerKey::DefaultSavePath))
			DefaultSavePath = PathsObj[ObjViewerKey::DefaultSavePath].ToString();
	}
}
