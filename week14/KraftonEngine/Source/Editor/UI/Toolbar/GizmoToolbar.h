#pragma once
#include "Core/CoreTypes.h"

#include <functional>

class FRenderer;
class UGizmoComponent;
struct FGizmoToolSettings;

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
	Count
};

struct FGizmoToolbarContext
{
	FRenderer* Renderer = nullptr;
	UGizmoComponent* Gizmo = nullptr;
	FGizmoToolSettings* Settings = nullptr;

	float ToolbarLeft = 0.0f;
	float ToolbarTop = 0.0f;

	bool bReservePlayStopSpace = true;
	bool bShowAddActor = true;
	bool bShowSnapControls = true;

	std::function<void()> OnAddActorClicked;
	std::function<void()> OnCoordSystemToggled;
	std::function<void()> OnSettingsChanged;
};

class FGizmoToolbar
{
public:
	static void Render(const FGizmoToolbarContext& Context);
};