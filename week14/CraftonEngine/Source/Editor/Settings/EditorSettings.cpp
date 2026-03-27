#include "Editor/Settings/EditorSettings.h"
#include "SimpleJSON/json.hpp"

#include <fstream>
#include <filesystem>

namespace Key
{
	// Section
	constexpr const char* Viewport = "Viewport";
	constexpr const char* Paths = "Paths";

	// Viewport
	constexpr const char* CameraSpeed = "CameraSpeed";
	constexpr const char* CameraRotationSpeed = "CameraRotationSpeed";
	constexpr const char* CameraZoomSpeed = "CameraZoomSpeed";
	constexpr const char* CameraMoveSensitivity = "CameraMoveSensitivity";
	constexpr const char* CameraRotateSensitivity = "CameraRotateSensitivity";
	constexpr const char* InitViewPos = "InitViewPos";
	constexpr const char* InitLookAt = "InitLookAt";

	// View
	constexpr const char* View = "View";
	constexpr const char* ViewMode = "ViewMode";
	constexpr const char* bPrimitives = "bPrimitives";
	constexpr const char* bGrid = "bGrid";
	constexpr const char* bGizmo = "bGizmo";
	constexpr const char* bBillboardText = "bBillboardText";
	constexpr const char* bBoundingVolume = "bBoundingVolume";

	// Grid
	constexpr const char* Grid = "Grid";
	constexpr const char* GridSpacing = "GridSpacing";
	constexpr const char* GridHalfLineCount = "GridHalfLineCount";

	// Paths
	constexpr const char* DefaultSavePath = "DefaultSavePath";
}

void FEditorSettings::SaveToFile(const FString& Path) const
{
	using namespace json;

	JSON Root = Object();

	// Viewport
	JSON Viewport = Object();
	Viewport[Key::CameraSpeed] = CameraSpeed;
	Viewport[Key::CameraRotationSpeed] = CameraRotationSpeed;
	Viewport[Key::CameraZoomSpeed] = CameraZoomSpeed;
	Viewport[Key::CameraMoveSensitivity] = CameraMoveSensitivity;
	Viewport[Key::CameraRotateSensitivity] = CameraRotateSensitivity;

	JSON InitPos = Array(InitViewPos.X, InitViewPos.Y, InitViewPos.Z);
	Viewport[Key::InitViewPos] = InitPos;

	JSON LookAt = Array(InitLookAt.X, InitLookAt.Y, InitLookAt.Z);
	Viewport[Key::InitLookAt] = LookAt;

	Root[Key::Viewport] = Viewport;

	// View
	JSON ViewObj = Object();
	ViewObj[Key::ViewMode] = static_cast<int32>(ViewMode);
	ViewObj[Key::bPrimitives] = ShowFlags.bPrimitives;
	ViewObj[Key::bGrid] = ShowFlags.bGrid;
	ViewObj[Key::bGizmo] = ShowFlags.bGizmo;
	ViewObj[Key::bBillboardText] = ShowFlags.bBillboardText;
	ViewObj[Key::bBoundingVolume] = ShowFlags.bBoundingVolume;
	Root[Key::View] = ViewObj;

	// Grid
	JSON GridObj = Object();
	GridObj[Key::GridSpacing] = GridSpacing;
	GridObj[Key::GridHalfLineCount] = GridHalfLineCount;
	Root[Key::Grid] = GridObj;

	// Paths
	JSON PathsObj = Object();
	PathsObj[Key::DefaultSavePath] = DefaultSavePath;
	Root[Key::Paths] = PathsObj;

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
	if (Root.hasKey(Key::Viewport))
	{
		JSON Viewport = Root[Key::Viewport];

		if (Viewport.hasKey(Key::CameraSpeed))
			CameraSpeed = static_cast<float>(Viewport[Key::CameraSpeed].ToFloat());
		if (Viewport.hasKey(Key::CameraRotationSpeed))
			CameraRotationSpeed = static_cast<float>(Viewport[Key::CameraRotationSpeed].ToFloat());
		if (Viewport.hasKey(Key::CameraZoomSpeed))
			CameraZoomSpeed = static_cast<float>(Viewport[Key::CameraZoomSpeed].ToFloat());
		if (Viewport.hasKey(Key::CameraMoveSensitivity))
			CameraMoveSensitivity = static_cast<float>(Viewport[Key::CameraMoveSensitivity].ToFloat());
		if (Viewport.hasKey(Key::CameraRotateSensitivity))
			CameraRotateSensitivity = static_cast<float>(Viewport[Key::CameraRotateSensitivity].ToFloat());

		if (Viewport.hasKey(Key::InitViewPos))
		{
			JSON Pos = Viewport[Key::InitViewPos];
			InitViewPos = FVector(
				static_cast<float>(Pos[0].ToFloat()),
				static_cast<float>(Pos[1].ToFloat()),
				static_cast<float>(Pos[2].ToFloat()));
		}

		if (Viewport.hasKey(Key::InitLookAt))
		{
			JSON Look = Viewport[Key::InitLookAt];
			InitLookAt = FVector(
				static_cast<float>(Look[0].ToFloat()),
				static_cast<float>(Look[1].ToFloat()),
				static_cast<float>(Look[2].ToFloat()));
		}
	}

	// View
	if (Root.hasKey(Key::View))
	{
		JSON ViewObj = Root[Key::View];

		if (ViewObj.hasKey(Key::ViewMode))
		{
			int32 Mode = ViewObj[Key::ViewMode].ToInt();
			if (Mode >= 0 && Mode < static_cast<int32>(EViewMode::Count))
				ViewMode = static_cast<EViewMode>(Mode);
		}
		if (ViewObj.hasKey(Key::bPrimitives))
			ShowFlags.bPrimitives = ViewObj[Key::bPrimitives].ToBool();
		if (ViewObj.hasKey(Key::bGrid))
			ShowFlags.bGrid = ViewObj[Key::bGrid].ToBool();
		if (ViewObj.hasKey(Key::bGizmo))
			ShowFlags.bGizmo = ViewObj[Key::bGizmo].ToBool();
		if (ViewObj.hasKey(Key::bBillboardText))
			ShowFlags.bBillboardText = ViewObj[Key::bBillboardText].ToBool();
		if (ViewObj.hasKey(Key::bBoundingVolume))
			ShowFlags.bBoundingVolume = ViewObj[Key::bBoundingVolume].ToBool();
	}

	// Grid
	if (Root.hasKey(Key::Grid))
	{
		JSON GridObj = Root[Key::Grid];

		if (GridObj.hasKey(Key::GridSpacing))
			GridSpacing = static_cast<float>(GridObj[Key::GridSpacing].ToFloat());
		if (GridObj.hasKey(Key::GridHalfLineCount))
			GridHalfLineCount = GridObj[Key::GridHalfLineCount].ToInt();
	}

	// Paths
	if (Root.hasKey(Key::Paths))
	{
		JSON PathsObj = Root[Key::Paths];

		if (PathsObj.hasKey(Key::DefaultSavePath))
			DefaultSavePath = PathsObj[Key::DefaultSavePath].ToString();
	}
}
