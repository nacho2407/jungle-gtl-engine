#pragma once

#include "Viewport/CursorOverlayState.h"
#include "Editor/Settings/EditorSettings.h"

class UWorld;
class UCameraComponent;
class UGizmoComponent;
class UPrimitiveComponent;

struct FRenderCollectorContext
{
	UWorld* World = nullptr;
	UCameraComponent* Camera = nullptr;
	UGizmoComponent* Gizmo = nullptr;
	const FCursorOverlayState* CursorOverlayState = nullptr;

	UPrimitiveComponent* SelectedComponent = nullptr;

	float ViewportWidth = 0.f;
	float ViewportHeight = 0.f;

	EViewMode ViewMode = EViewMode::Lit;
	FShowFlags ShowFlags;
};
