#include "Editor/Settings/EditorSettings.h"
#include "Editor/Viewport/FLevelViewportLayout.h"
#include "SimpleJSON/json.hpp"

#include <fstream>
#include <filesystem>

namespace Key
{
	// Section
	constexpr const char* Viewport = "Viewport";
	constexpr const char* MeshEditorViewport = "MeshEditorViewport";
	constexpr const char* Paths = "Paths";

	// Viewport
	constexpr const char* CameraSpeed = "CameraSpeed";
	constexpr const char* CameraRotationSpeed = "CameraRotationSpeed";
	constexpr const char* CameraZoomSpeed = "CameraZoomSpeed";
	constexpr const char* InitViewPos = "InitViewPos";
	constexpr const char* InitLookAt = "InitLookAt";

	// Slot Render Options
	constexpr const char* ViewMode = "ViewMode";
	constexpr const char* bStaticMesh = "bStaticMesh";
	constexpr const char* bSkeletalMesh = "bSkeletalMesh";
	constexpr const char* bGrid = "bGrid";
	constexpr const char* bWorldAxis = "bWorldAxis";
	constexpr const char* bGizmo = "bGizmo";
	constexpr const char* bBillboardText = "bBillboardText";
	constexpr const char* bBoundingVolume = "bBoundingVolume";
	constexpr const char* bDebugDraw = "bDebugDraw";
	constexpr const char* bOctree = "bOctree";
	constexpr const char* bFog = "bFog";
	constexpr const char* bFXAA = "bFXAA";
	constexpr const char* bGammaCorrection = "bGammaCorrection";
	constexpr const char* bViewLightCulling = "bViewLightCulling";
	constexpr const char* bVisualize25DCulling = "bVisualize25DCulling";
	constexpr const char* bShowShadowFrustum = "bShowShadowFrustum";
	constexpr const char* bCollision = "bCollision";
	constexpr const char* bShowCollisionShape = "bShowCollisionShape";
	constexpr const char* GridSpacing = "GridSpacing";
	constexpr const char* GridHalfLineCount = "GridHalfLineCount";
	constexpr const char* CameraMoveSensitivity = "CameraMoveSensitivity";
	constexpr const char* CameraRotateSensitivity = "CameraRotateSensitivity";
	constexpr const char* SceneDepthVisMode = "SceneDepthVisMode";
	constexpr const char* Exponent = "Exponent";
	constexpr const char* Range = "Range";
	constexpr const char* EdgeThreshold = "EdgeThreshold";
	constexpr const char* EdgeThresholdMin = "EdgeThresholdMin";
	constexpr const char* Gamma = "Gamma";
	constexpr const char* LightCullingMode = "LightCullingMode";
	constexpr const char* HeatMapMax = "HeatMapMax";
	constexpr const char* Enable25DCulling = "Enable25DCulling";

	// Paths
	constexpr const char* EditorStartLevel = "EditorStartLevel";
	constexpr const char* ContentBrowserPath = "ContentBrowserPath";

	// Layout
	constexpr const char* Layout = "Layout";
	constexpr const char* LayoutType = "LayoutType";
	constexpr const char* Slots = "Slots";
	constexpr const char* ViewportType = "ViewportType";
	constexpr const char* SplitterRatios = "SplitterRatios";

	// UI Widgets
	constexpr const char* UIWidgets = "UIWidgets";
	constexpr const char* ShowConsole = "ShowConsole";
	constexpr const char* ShowControlPanel = "ShowControlPanel";
	constexpr const char* ShowPropertyWindow = "ShowPropertyWindow";
	constexpr const char* ShowSceneManager = "ShowSceneManager";
	constexpr const char* ShowStatProfiler = "ShowStatProfiler";
	constexpr const char* ShowContentBrowser = "ShowContentBrowser";
	constexpr const char* ShowImGuiSettings = "ShowImGuiSettings";
	constexpr const char* ShowEditorDebug = "ShowEditorDebug";
	constexpr const char* ShowShadowMapDebug = "ShowShadowMapDebug";

	// Perspective Camera
	constexpr const char* PerspectiveCamera = "PerspectiveCamera";
	constexpr const char* Location = "Location";
	constexpr const char* Rotation = "Rotation";
	constexpr const char* FOV = "FOV";
	constexpr const char* NearClip = "NearClip";
	constexpr const char* FarClip = "FarClip";

	// Transform Tools
	constexpr const char* TransformTools = "TransformTools";
	constexpr const char* MeshEditorTransformTools = "MeshEditorTransformTools";

	constexpr const char* CoordSystem = "CoordSystem";
	constexpr const char* bEnableTranslationSnap = "bEnableTranslationSnap";
	constexpr const char* TranslationSnapSize = "TranslationSnapSize";
	constexpr const char* bEnableRotationSnap = "bEnableRotationSnap";
	constexpr const char* RotationSnapSize = "RotationSnapSize";
	constexpr const char* bEnableScaleSnap = "bEnableScaleSnap";
	constexpr const char* ScaleSnapSize = "ScaleSnapSize";
}

namespace
{
json::JSON SaveCameraControls(const FViewportCameraControlSettings& Camera)
{
	using namespace json;

	JSON Obj = Object();
	Obj[Key::CameraSpeed] = Camera.MoveSpeed;
	Obj[Key::CameraRotationSpeed] = Camera.RotationSpeed;
	Obj[Key::CameraZoomSpeed] = Camera.ZoomSpeed;
	return Obj;
}

void LoadCameraControls(json::JSON Obj, FViewportCameraControlSettings& Camera)
{
	if (Obj.hasKey(Key::CameraSpeed))
		Camera.MoveSpeed = static_cast<float>(Obj[Key::CameraSpeed].ToFloat());
	if (Obj.hasKey(Key::CameraRotationSpeed))
		Camera.RotationSpeed = static_cast<float>(Obj[Key::CameraRotationSpeed].ToFloat());
	if (Obj.hasKey(Key::CameraZoomSpeed))
		Camera.ZoomSpeed = static_cast<float>(Obj[Key::CameraZoomSpeed].ToFloat());
}

json::JSON SaveRenderOptions(const FViewportRenderOptions& Opts)
{
	using namespace json;

	JSON Obj = Object();
	Obj[Key::ViewMode] = static_cast<int32>(Opts.ViewMode);
	Obj[Key::ViewportType] = static_cast<int32>(Opts.ViewportType);
	Obj[Key::bStaticMesh] = Opts.ShowFlags.bStaticMesh;
	Obj[Key::bSkeletalMesh] = Opts.ShowFlags.bSkeletalMesh;
	Obj[Key::bGrid] = Opts.ShowFlags.bGrid;
	Obj[Key::bWorldAxis] = Opts.ShowFlags.bWorldAxis;
	Obj[Key::bGizmo] = Opts.ShowFlags.bGizmo;
	Obj[Key::bBillboardText] = Opts.ShowFlags.bBillboardText;
	Obj[Key::bBoundingVolume] = Opts.ShowFlags.bBoundingVolume;
	Obj[Key::bDebugDraw] = Opts.ShowFlags.bDebugDraw;
	Obj[Key::bOctree] = Opts.ShowFlags.bOctree;
	Obj[Key::bFog] = Opts.ShowFlags.bFog;
	Obj[Key::bFXAA] = Opts.ShowFlags.bFXAA;
	Obj[Key::bGammaCorrection] = Opts.ShowFlags.bGammaCorrection;
	Obj[Key::bViewLightCulling] = Opts.ShowFlags.bViewLightCulling;
	Obj[Key::bVisualize25DCulling] = Opts.ShowFlags.bVisualize25DCulling;
	Obj[Key::bShowShadowFrustum] = Opts.ShowFlags.bShowShadowFrustum;
	Obj[Key::bCollision] = Opts.ShowFlags.bCollision;
	Obj[Key::bShowCollisionShape] = Opts.ShowFlags.bShowCollisionShape;
	Obj[Key::GridSpacing] = Opts.GridSpacing;
	Obj[Key::GridHalfLineCount] = Opts.GridHalfLineCount;
	Obj[Key::CameraMoveSensitivity] = Opts.CameraMoveSensitivity;
	Obj[Key::CameraRotateSensitivity] = Opts.CameraRotateSensitivity;
	Obj[Key::SceneDepthVisMode] = Opts.SceneDepthVisMode;
	Obj[Key::Exponent] = Opts.Exponent;
	Obj[Key::Range] = Opts.Range;
	Obj[Key::EdgeThreshold] = Opts.EdgeThreshold;
	Obj[Key::EdgeThresholdMin] = Opts.EdgeThresholdMin;
	Obj[Key::Gamma] = Opts.Gamma;
	Obj[Key::LightCullingMode] = static_cast<int32>(Opts.LightCullingMode);
	Obj[Key::HeatMapMax] = Opts.HeatMapMax;
	Obj[Key::Enable25DCulling] = Opts.Enable25DCulling;
	return Obj;
}

void LoadRenderOptions(json::JSON Obj, FViewportRenderOptions& Opts)
{
	if (Obj.hasKey(Key::ViewMode))
		Opts.ViewMode = static_cast<EViewMode>(Obj[Key::ViewMode].ToInt());
	if (Obj.hasKey(Key::ViewportType))
		Opts.ViewportType = static_cast<ELevelViewportType>(Obj[Key::ViewportType].ToInt());
	if (Obj.hasKey(Key::bStaticMesh))
		Opts.ShowFlags.bStaticMesh = Obj[Key::bStaticMesh].ToBool();
	if (Obj.hasKey(Key::bSkeletalMesh))
		Opts.ShowFlags.bSkeletalMesh = Obj[Key::bSkeletalMesh].ToBool();
	if (Obj.hasKey(Key::bGrid))
		Opts.ShowFlags.bGrid = Obj[Key::bGrid].ToBool();
	if (Obj.hasKey(Key::bWorldAxis))
		Opts.ShowFlags.bWorldAxis = Obj[Key::bWorldAxis].ToBool();
	if (Obj.hasKey(Key::bGizmo))
		Opts.ShowFlags.bGizmo = Obj[Key::bGizmo].ToBool();
	if (Obj.hasKey(Key::bBillboardText))
		Opts.ShowFlags.bBillboardText = Obj[Key::bBillboardText].ToBool();
	if (Obj.hasKey(Key::bBoundingVolume))
		Opts.ShowFlags.bBoundingVolume = Obj[Key::bBoundingVolume].ToBool();
	if (Obj.hasKey(Key::bDebugDraw))
		Opts.ShowFlags.bDebugDraw = Obj[Key::bDebugDraw].ToBool();
	if (Obj.hasKey(Key::bOctree))
		Opts.ShowFlags.bOctree = Obj[Key::bOctree].ToBool();
	if (Obj.hasKey(Key::bFog))
		Opts.ShowFlags.bFog = Obj[Key::bFog].ToBool();
	if (Obj.hasKey(Key::bFXAA))
		Opts.ShowFlags.bFXAA = Obj[Key::bFXAA].ToBool();
	if (Obj.hasKey(Key::bGammaCorrection))
		Opts.ShowFlags.bGammaCorrection = Obj[Key::bGammaCorrection].ToBool();
	if (Obj.hasKey(Key::bViewLightCulling))
		Opts.ShowFlags.bViewLightCulling = Obj[Key::bViewLightCulling].ToBool();
	if (Obj.hasKey(Key::bVisualize25DCulling))
		Opts.ShowFlags.bVisualize25DCulling = Obj[Key::bVisualize25DCulling].ToBool();
	if (Obj.hasKey(Key::bShowShadowFrustum))
		Opts.ShowFlags.bShowShadowFrustum = Obj[Key::bShowShadowFrustum].ToBool();
	if (Obj.hasKey(Key::bCollision))
		Opts.ShowFlags.bCollision = Obj[Key::bCollision].ToBool();
	if (Obj.hasKey(Key::bShowCollisionShape))
		Opts.ShowFlags.bShowCollisionShape = Obj[Key::bShowCollisionShape].ToBool();
	if (Obj.hasKey(Key::GridSpacing))
		Opts.GridSpacing = static_cast<float>(Obj[Key::GridSpacing].ToFloat());
	if (Obj.hasKey(Key::GridHalfLineCount))
		Opts.GridHalfLineCount = Obj[Key::GridHalfLineCount].ToInt();
	if (Obj.hasKey(Key::CameraMoveSensitivity))
		Opts.CameraMoveSensitivity = static_cast<float>(Obj[Key::CameraMoveSensitivity].ToFloat());
	if (Obj.hasKey(Key::CameraRotateSensitivity))
		Opts.CameraRotateSensitivity = static_cast<float>(Obj[Key::CameraRotateSensitivity].ToFloat());
	if (Obj.hasKey(Key::SceneDepthVisMode))
		Opts.SceneDepthVisMode = Obj[Key::SceneDepthVisMode].ToInt();
	if (Obj.hasKey(Key::Exponent))
		Opts.Exponent = static_cast<float>(Obj[Key::Exponent].ToFloat());
	if (Obj.hasKey(Key::Range))
		Opts.Range = static_cast<float>(Obj[Key::Range].ToFloat());
	if (Obj.hasKey(Key::EdgeThreshold))
		Opts.EdgeThreshold = static_cast<float>(Obj[Key::EdgeThreshold].ToFloat());
	if (Obj.hasKey(Key::EdgeThresholdMin))
		Opts.EdgeThresholdMin = static_cast<float>(Obj[Key::EdgeThresholdMin].ToFloat());
	if (Obj.hasKey(Key::Gamma))
		Opts.Gamma = static_cast<float>(Obj[Key::Gamma].ToFloat());
	if (Obj.hasKey(Key::LightCullingMode))
		Opts.LightCullingMode = static_cast<ELightCullingMode>(Obj[Key::LightCullingMode].ToInt());
	if (Obj.hasKey(Key::HeatMapMax))
		Opts.HeatMapMax = static_cast<float>(Obj[Key::HeatMapMax].ToFloat());
	if (Obj.hasKey(Key::Enable25DCulling))
		Opts.Enable25DCulling = Obj[Key::Enable25DCulling].ToBool();
}

json::JSON SaveGizmoSettings(const FGizmoToolSettings& Gizmo)
{
	using namespace json;

	JSON Obj = Object();
	Obj[Key::CoordSystem] = static_cast<int32>(Gizmo.CoordSystem);
	Obj[Key::bEnableTranslationSnap] = Gizmo.bEnableTranslationSnap;
	Obj[Key::TranslationSnapSize] = Gizmo.TranslationSnapSize;
	Obj[Key::bEnableRotationSnap] = Gizmo.bEnableRotationSnap;
	Obj[Key::RotationSnapSize] = Gizmo.RotationSnapSize;
	Obj[Key::bEnableScaleSnap] = Gizmo.bEnableScaleSnap;
	Obj[Key::ScaleSnapSize] = Gizmo.ScaleSnapSize;
	return Obj;
}

void LoadGizmoSettings(json::JSON Obj, FGizmoToolSettings& Gizmo)
{
	if (Obj.hasKey(Key::CoordSystem))
		Gizmo.CoordSystem = static_cast<EEditorCoordSystem>(Obj[Key::CoordSystem].ToInt());
	if (Obj.hasKey(Key::bEnableTranslationSnap))
		Gizmo.bEnableTranslationSnap = Obj[Key::bEnableTranslationSnap].ToBool();
	if (Obj.hasKey(Key::TranslationSnapSize))
		Gizmo.TranslationSnapSize = static_cast<float>(Obj[Key::TranslationSnapSize].ToFloat());
	if (Obj.hasKey(Key::bEnableRotationSnap))
		Gizmo.bEnableRotationSnap = Obj[Key::bEnableRotationSnap].ToBool();
	if (Obj.hasKey(Key::RotationSnapSize))
		Gizmo.RotationSnapSize = static_cast<float>(Obj[Key::RotationSnapSize].ToFloat());
	if (Obj.hasKey(Key::bEnableScaleSnap))
		Gizmo.bEnableScaleSnap = Obj[Key::bEnableScaleSnap].ToBool();
	if (Obj.hasKey(Key::ScaleSnapSize))
		Gizmo.ScaleSnapSize = static_cast<float>(Obj[Key::ScaleSnapSize].ToFloat());
}
}

void FEditorSettings::SaveToFile(const FString& Path) const
{
	using namespace json;

	JSON Root = Object();

	// Viewport
	JSON Viewport = SaveCameraControls(LevelViewportCameraControls);

	JSON InitPos = Array(InitViewPos.X, InitViewPos.Y, InitViewPos.Z);
	Viewport[Key::InitViewPos] = InitPos;

	JSON LookAt = Array(InitLookAt.X, InitLookAt.Y, InitLookAt.Z);
	Viewport[Key::InitLookAt] = LookAt;

	Root[Key::Viewport] = Viewport;

	// Paths
	JSON PathsObj = Object();
	PathsObj[Key::EditorStartLevel] = EditorStartLevel;
	PathsObj[Key::ContentBrowserPath] = ContentBrowserPath;
	Root[Key::Paths] = PathsObj;

	// Layout
	JSON LayoutObj = Object();
	LayoutObj[Key::LayoutType] = LayoutType;

	JSON SlotsArr = Array();
	int32 SlotCount = FLevelViewportLayout::GetSlotCount(static_cast<EViewportLayout>(LayoutType));
	for (int32 i = 0; i < SlotCount; ++i)
	{
		JSON SlotObj = Object();
		const FViewportRenderOptions& Opts = LevelViewportSettings[i].RenderOptions;
		SlotObj = SaveRenderOptions(Opts);
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

	// UI Widgets
	JSON WidgetsObj = Object();
	WidgetsObj[Key::ShowConsole] = UI.bConsole;
	WidgetsObj[Key::ShowControlPanel] = UI.bControl;
	WidgetsObj[Key::ShowPropertyWindow] = UI.bProperty;
	WidgetsObj[Key::ShowSceneManager] = UI.bScene;
	WidgetsObj[Key::ShowStatProfiler] = UI.bStat;
	WidgetsObj[Key::ShowContentBrowser] = UI.bContentBrowser;
	WidgetsObj[Key::ShowImGuiSettings] = UI.bImGUISettings;
	WidgetsObj[Key::ShowEditorDebug] = UI.bEditorDebug;
	WidgetsObj[Key::ShowShadowMapDebug] = UI.bShadowMapDebug;
	Root[Key::UIWidgets] = WidgetsObj;

	// Perspective Camera
	JSON CamObj = Object();
	CamObj[Key::Location] = Array(PerspCamLocation.X, PerspCamLocation.Y, PerspCamLocation.Z);
	CamObj[Key::Rotation] = Array(PerspCamRotation.Roll, PerspCamRotation.Pitch, PerspCamRotation.Yaw);
	CamObj[Key::FOV] = PerspCamFOV;
	CamObj[Key::NearClip] = PerspCamNearClip;
	CamObj[Key::FarClip] = PerspCamFarClip;
	Root[Key::PerspectiveCamera] = CamObj;

	Root[Key::TransformTools] = SaveGizmoSettings(LevelViewportSettings[0].Gizmo);

	JSON MeshEditorViewportObj = SaveRenderOptions(MeshEditorViewportSettings.RenderOptions);
	JSON MeshEditorCameraObj = SaveCameraControls(MeshEditorViewportSettings.CameraControls);
	MeshEditorViewportObj[Key::CameraSpeed] = MeshEditorCameraObj[Key::CameraSpeed];
	MeshEditorViewportObj[Key::CameraRotationSpeed] = MeshEditorCameraObj[Key::CameraRotationSpeed];
	MeshEditorViewportObj[Key::CameraZoomSpeed] = MeshEditorCameraObj[Key::CameraZoomSpeed];
	Root[Key::MeshEditorViewport] = MeshEditorViewportObj;
	Root[Key::MeshEditorTransformTools] = SaveGizmoSettings(MeshEditorViewportSettings.Gizmo);

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

		LoadCameraControls(Viewport, LevelViewportCameraControls);

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

		if (PathsObj.hasKey(Key::EditorStartLevel))
			EditorStartLevel = PathsObj[Key::EditorStartLevel].ToString();
		if (PathsObj.hasKey(Key::ContentBrowserPath))
			ContentBrowserPath = PathsObj[Key::ContentBrowserPath].ToString();
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
				FViewportRenderOptions& Opts = LevelViewportSettings[i].RenderOptions;
				LoadRenderOptions(S, Opts);
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

	// UI Widgets
	if (Root.hasKey(Key::UIWidgets))
	{
		JSON W = Root[Key::UIWidgets];
		if (W.hasKey(Key::ShowConsole))        UI.bConsole = W[Key::ShowConsole].ToBool();
		if (W.hasKey(Key::ShowControlPanel))   UI.bControl = W[Key::ShowControlPanel].ToBool();
		if (W.hasKey(Key::ShowPropertyWindow)) UI.bProperty = W[Key::ShowPropertyWindow].ToBool();
		if (W.hasKey(Key::ShowSceneManager))   UI.bScene = W[Key::ShowSceneManager].ToBool();
		if (W.hasKey(Key::ShowStatProfiler))   UI.bStat = W[Key::ShowStatProfiler].ToBool();
		if (W.hasKey(Key::ShowContentBrowser)) UI.bContentBrowser = W[Key::ShowContentBrowser].ToBool();
		if (W.hasKey(Key::ShowImGuiSettings))  UI.bImGUISettings = W[Key::ShowImGuiSettings].ToBool();
		if (W.hasKey(Key::ShowEditorDebug))    UI.bEditorDebug = W[Key::ShowEditorDebug].ToBool();
		if (W.hasKey(Key::ShowShadowMapDebug)) UI.bShadowMapDebug = W[Key::ShowShadowMapDebug].ToBool();
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

	if (Root.hasKey(Key::TransformTools))
	{
		JSON TransformObj = Root[Key::TransformTools];
		LoadGizmoSettings(TransformObj, LevelViewportSettings[0].Gizmo);
	}

	if (Root.hasKey(Key::MeshEditorViewport))
	{
		JSON MeshEditorViewportObj = Root[Key::MeshEditorViewport];
		LoadRenderOptions(MeshEditorViewportObj, MeshEditorViewportSettings.RenderOptions);
		LoadCameraControls(MeshEditorViewportObj, MeshEditorViewportSettings.CameraControls);
	}

	if (Root.hasKey(Key::MeshEditorTransformTools))
	{
		JSON MeshEditorTransformObj = Root[Key::MeshEditorTransformTools];
		LoadGizmoSettings(MeshEditorTransformObj, MeshEditorViewportSettings.Gizmo);
	}
}
