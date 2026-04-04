#include "Editor/Settings/EditorSettings.h"
#include "Editor/Viewport/FLevelViewportLayout.h"
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
	constexpr const char* InitViewPos = "InitViewPos";
	constexpr const char* InitLookAt = "InitLookAt";

	// Slot Render Options
	constexpr const char* ViewMode = "ViewMode";
	constexpr const char* bPrimitives = "bPrimitives";
	constexpr const char* bGrid = "bGrid";
	constexpr const char* bGizmo = "bGizmo";
	constexpr const char* bBillboardText = "bBillboardText";
	constexpr const char* bBoundingVolume = "bBoundingVolume";
	constexpr const char* GridSpacing = "GridSpacing";
	constexpr const char* GridHalfLineCount = "GridHalfLineCount";
	constexpr const char* CameraMoveSensitivity = "CameraMoveSensitivity";
	constexpr const char* CameraRotateSensitivity = "CameraRotateSensitivity";

	// Paths
	constexpr const char* DefaultSavePath = "DefaultSavePath";

	// Layout
	constexpr const char* Layout = "Layout";
	constexpr const char* LayoutType = "LayoutType";
	constexpr const char* Slots = "Slots";
	constexpr const char* ViewportType = "ViewportType";
	constexpr const char* SplitterRatios = "SplitterRatios";

	// Runtime
	constexpr const char* Runtime = "Runtime";
	constexpr const char* RunTimeOptimization = "RunTimeOptimization";

	// Perspective Camera
	constexpr const char* PerspectiveCamera = "PerspectiveCamera";
	constexpr const char* Location = "Location";
	constexpr const char* Rotation = "Rotation";
	constexpr const char* FOV = "FOV";
	constexpr const char* NearClip = "NearClip";
	constexpr const char* FarClip = "FarClip";
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

	JSON InitPos = Array(InitViewPos.X, InitViewPos.Y, InitViewPos.Z);
	Viewport[Key::InitViewPos] = InitPos;

	JSON LookAt = Array(InitLookAt.X, InitLookAt.Y, InitLookAt.Z);
	Viewport[Key::InitLookAt] = LookAt;

	Root[Key::Viewport] = Viewport;

	// Paths
	JSON PathsObj = Object();
	PathsObj[Key::DefaultSavePath] = DefaultSavePath;
	Root[Key::Paths] = PathsObj;

	// Layout
	JSON LayoutObj = Object();
	LayoutObj[Key::LayoutType] = LayoutType;

	JSON SlotsArr = Array();
	int32 SlotCount = FLevelViewportLayout::GetSlotCount(static_cast<EViewportLayout>(LayoutType));
	for (int32 i = 0; i < SlotCount; ++i)
	{
		JSON SlotObj = Object();
		const FViewportRenderOptions& Opts = SlotOptions[i];
		SlotObj[Key::ViewMode] = static_cast<int32>(Opts.ViewMode);
		SlotObj[Key::ViewportType] = static_cast<int32>(Opts.ViewportType);
		SlotObj[Key::bPrimitives] = Opts.ShowFlags.bPrimitives;
		SlotObj[Key::bGrid] = Opts.ShowFlags.bGrid;
		SlotObj[Key::bGizmo] = Opts.ShowFlags.bGizmo;
		SlotObj[Key::bBillboardText] = Opts.ShowFlags.bBillboardText;
		SlotObj[Key::bBoundingVolume] = Opts.ShowFlags.bBoundingVolume;
		SlotObj[Key::GridSpacing] = Opts.GridSpacing;
		SlotObj[Key::GridHalfLineCount] = Opts.GridHalfLineCount;
		SlotObj[Key::CameraMoveSensitivity] = Opts.CameraMoveSensitivity;
		SlotObj[Key::CameraRotateSensitivity] = Opts.CameraRotateSensitivity;
		SlotsArr.append(SlotObj);
	}
	LayoutObj[Key::Slots] = SlotsArr;

	JSON RatiosArr = Array();
	for (int32 i = 0; i < SplitterCount; ++i)
	{
		RatiosArr.append(SplitterRatios[i]);
	}
	LayoutObj[Key::SplitterRatios] = RatiosArr;
	Root[Key::Layout] = LayoutObj;

	// Runtime
	JSON RuntimeObj = Object();
	RuntimeObj[Key::RunTimeOptimization] = bRunTimeOptimization;
	Root[Key::Runtime] = RuntimeObj;

	// Perspective Camera
	JSON CamObj = Object();
	CamObj[Key::Location] = Array(PerspCamLocation.X, PerspCamLocation.Y, PerspCamLocation.Z);
	CamObj[Key::Rotation] = Array(PerspCamRotation.Roll, PerspCamRotation.Pitch, PerspCamRotation.Yaw);
	CamObj[Key::FOV] = PerspCamFOV;
	CamObj[Key::NearClip] = PerspCamNearClip;
	CamObj[Key::FarClip] = PerspCamFarClip;
	Root[Key::PerspectiveCamera] = CamObj;

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

	// Paths
	if (Root.hasKey(Key::Paths))
	{
		JSON PathsObj = Root[Key::Paths];

		if (PathsObj.hasKey(Key::DefaultSavePath))
			DefaultSavePath = PathsObj[Key::DefaultSavePath].ToString();
	}

	// Layout
	if (Root.hasKey(Key::Layout))
	{
		JSON LayoutObj = Root[Key::Layout];

		if (LayoutObj.hasKey(Key::LayoutType))
			LayoutType = LayoutObj[Key::LayoutType].ToInt();

		if (LayoutObj.hasKey(Key::Slots))
		{
			JSON SlotsArr = LayoutObj[Key::Slots];
			for (int32 i = 0; i < static_cast<int32>(SlotsArr.length()) && i < 4; ++i)
			{
				JSON S = SlotsArr[i];
				FViewportRenderOptions& Opts = SlotOptions[i];
				if (S.hasKey(Key::ViewMode))
					Opts.ViewMode = static_cast<EViewMode>(S[Key::ViewMode].ToInt());
				if (S.hasKey(Key::ViewportType))
					Opts.ViewportType = static_cast<ELevelViewportType>(S[Key::ViewportType].ToInt());
				if (S.hasKey(Key::bPrimitives))
					Opts.ShowFlags.bPrimitives = S[Key::bPrimitives].ToBool();
				if (S.hasKey(Key::bGrid))
					Opts.ShowFlags.bGrid = S[Key::bGrid].ToBool();
				if (S.hasKey(Key::bGizmo))
					Opts.ShowFlags.bGizmo = S[Key::bGizmo].ToBool();
				if (S.hasKey(Key::bBillboardText))
					Opts.ShowFlags.bBillboardText = S[Key::bBillboardText].ToBool();
				if (S.hasKey(Key::bBoundingVolume))
					Opts.ShowFlags.bBoundingVolume = S[Key::bBoundingVolume].ToBool();
				if (S.hasKey(Key::GridSpacing))
					Opts.GridSpacing = static_cast<float>(S[Key::GridSpacing].ToFloat());
				if (S.hasKey(Key::GridHalfLineCount))
					Opts.GridHalfLineCount = S[Key::GridHalfLineCount].ToInt();
				if (S.hasKey(Key::CameraMoveSensitivity))
					Opts.CameraMoveSensitivity = static_cast<float>(S[Key::CameraMoveSensitivity].ToFloat());
				if (S.hasKey(Key::CameraRotateSensitivity))
					Opts.CameraRotateSensitivity = static_cast<float>(S[Key::CameraRotateSensitivity].ToFloat());
			}
		}

		if (LayoutObj.hasKey(Key::SplitterRatios))
		{
			JSON RatiosArr = LayoutObj[Key::SplitterRatios];
			SplitterCount = static_cast<int32>(RatiosArr.length());
			if (SplitterCount > 3) SplitterCount = 3;
			for (int32 i = 0; i < SplitterCount; ++i)
			{
				SplitterRatios[i] = static_cast<float>(RatiosArr[i].ToFloat());
			}
		}
	}

	// Runtime
	if (Root.hasKey(Key::Runtime))
	{
		JSON RuntimeObj = Root[Key::Runtime];
		if (RuntimeObj.hasKey(Key::RunTimeOptimization))
			bRunTimeOptimization = RuntimeObj[Key::RunTimeOptimization].ToBool();
	}

	// Perspective Camera
	if (Root.hasKey(Key::PerspectiveCamera))
	{
		JSON CamObj = Root[Key::PerspectiveCamera];
		if (CamObj.hasKey(Key::Location))
		{
			JSON L = CamObj[Key::Location];
			PerspCamLocation = FVector(
				static_cast<float>(L[0].ToFloat()),
				static_cast<float>(L[1].ToFloat()),
				static_cast<float>(L[2].ToFloat()));
		}
		if (CamObj.hasKey(Key::Rotation))
		{
			JSON R = CamObj[Key::Rotation];
			// JSON 포맷: [Roll, Pitch, Yaw] (FVector X,Y,Z 호환)
			float Roll  = static_cast<float>(R[0].ToFloat());
			float Pitch = static_cast<float>(R[1].ToFloat());
			float Yaw   = static_cast<float>(R[2].ToFloat());
			PerspCamRotation = FRotator(Pitch, Yaw, Roll);
		}
		if (CamObj.hasKey(Key::FOV))
			PerspCamFOV = static_cast<float>(CamObj[Key::FOV].ToFloat());
		if (CamObj.hasKey(Key::NearClip))
			PerspCamNearClip = static_cast<float>(CamObj[Key::NearClip].ToFloat());
		if (CamObj.hasKey(Key::FarClip))
			PerspCamFarClip = static_cast<float>(CamObj[Key::FarClip].ToFloat());
	}
}
