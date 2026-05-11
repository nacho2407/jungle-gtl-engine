#pragma once
#include "Core/CoreTypes.h"
#include "Render/Types/ViewTypes.h"
#include "Settings/EditorViewportSettings.h"

#include <functional>

class FRenderer;
class UGizmoComponent;
struct ID3D11ShaderResourceView;

enum class EToolbarIcon : int32
{
	Menu = 0,
	Setting,
	AddActor,
	Translate,
	Rotate,
	Scale,
	WorldSpace,
	LocalSpace,
	TranslateSnap,
	RotateSnap,
	ScaleSnap,
	ShowFlag,
	Camera,
	Count
};

struct FViewportToolbarContext
{
	FRenderer* Renderer = nullptr;
	UGizmoComponent* Gizmo = nullptr;
	FEditorViewportSettings* Settings = nullptr;
	FViewportCameraControlSettings* CameraSettings = nullptr;
	FViewportRenderOptions* RenderOptions = nullptr;

	float ToolbarLeft = 0.0f;
	float ToolbarTop = 0.0f;
	float ToolbarWidth = 0.0f;

	bool bReservePlayStopSpace = false;

	bool bShowLayoutControls = false;
	bool bShowViewportType = false;
	bool bShowAddActor = false;
	bool bShowGizmoControls = true;
	bool bShowCameraControls = true;
	bool bShowViewMode = true;
	bool bShowShowFlags = true;

	int32 SlotIndex = 0;
	ID3D11ShaderResourceView** LayoutIcons = nullptr;
	int32 LayoutIconCount = 0;
	int32 CurrentLayoutIndex = -1;
	int32 ToggleLayoutIndex = -1;

	std::function<void()> OnAddActorClicked;
	std::function<void()> OnCoordSystemToggled;
	std::function<void()> OnSettingsChanged;

	std::function<void(int32)> OnLayoutSelected;
	std::function<void()> OnToggleLayout;
	std::function<void(ELevelViewportType)> OnViewportTypeSelected;
};

struct FToolbarRenderState
{
	const FViewportToolbarContext& Context;

	float FallbackIconSize = 14.0f;
	float MaxIconSize = 16.0f;
	float ButtonSpacing = 4.0f;
	float GroupSpacing = 12.0f;

	explicit FToolbarRenderState(const FViewportToolbarContext& InContext)
		: Context(InContext)
	{
	}

	FEditorViewportSettings& Settings() const { return *Context.Settings; }

	FViewportRenderOptions& RenderOptions() const { return Context.RenderOptions ? *Context.RenderOptions : Context.Settings->RenderOptions; }
	FGizmoToolSettings& GizmoSettings() const { return Context.Settings->Gizmo; }
	FViewportCameraControlSettings& CameraSettings() const { return Context.CameraSettings ? *Context.CameraSettings : Context.Settings->CameraControls; }
};

class FViewportToolbar
{
public:
	static void Render(const FViewportToolbarContext& Context);

private:
	static void BeginToolbar(const FToolbarRenderState& State);
	static void EndToolbar(const FToolbarRenderState& State);

	static void RenderLeftToolbarSection(const FToolbarRenderState& State);
	static void RenderRightToolbarSection(const FToolbarRenderState& State);

	static void RenderLayoutControls(const FToolbarRenderState& State);
	static void RenderAddActor(const FToolbarRenderState& State);
	static void RenderViewportType(const FToolbarRenderState& State);

	static void RenderGizmoControls(const FToolbarRenderState& State);
	static void RenderCoordSystemButton(const FToolbarRenderState& State);
	static void RenderSnapControls(const FToolbarRenderState& State);

	static void RenderCameraControls(const FToolbarRenderState& State);
	static void RenderViewMode(const FToolbarRenderState& State);
	static void RenderShowFlags(const FToolbarRenderState& State);
};
