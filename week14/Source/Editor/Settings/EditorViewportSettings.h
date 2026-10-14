#pragma once

#include "GizmoToolSettings.h"
#include "ViewportCameraControlSettings.h"
#include "Render/Types/ViewTypes.h"

struct FEditorViewportSettings
{
	FViewportRenderOptions RenderOptions;
	FViewportCameraControlSettings CameraControls;
	FGizmoToolSettings Gizmo;
};
