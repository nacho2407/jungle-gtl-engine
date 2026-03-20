#pragma once
#pragma once

#include "Viewport/CursorOverlayState.h"

class UWorld;
class UCamera;
class UGizmoComponent;
class UPrimitiveComponent;

struct FRenderCollectorContext
{
	UWorld* World = nullptr;
	UCamera* Camera = nullptr;
	UGizmoComponent* Gizmo = nullptr;
	const FCursorOverlayState* CursorOverlayState = nullptr;

	UPrimitiveComponent* SelectedComponent = nullptr;

	float ViewportWidth = 0.f;
	float ViewportHeight = 0.f;

	bool bGridVisible = true;
};
