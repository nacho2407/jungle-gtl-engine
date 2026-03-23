#pragma once

#include "Viewport/CursorOverlayState.h"
#include "Render/Common/ViewTypes.h"

class UWorld;
class UCameraComponent;
class UGizmoComponent;
class UPrimitiveComponent;
class AActor;

struct FRenderCollectorContext
{
	FRenderCollectorContext(const TArray<AActor*>& InSelected)
		: SelectedActors(InSelected)
	{
	}
	UWorld* World = nullptr;
	UCameraComponent* Camera = nullptr;
	UGizmoComponent* Gizmo = nullptr;
	const FCursorOverlayState* CursorOverlayState = nullptr;

	const TArray<AActor*>& SelectedActors;
	float ViewportWidth = 0.f;
	float ViewportHeight = 0.f;

	EViewMode ViewMode = EViewMode::Lit;
	FShowFlags ShowFlags;
};
