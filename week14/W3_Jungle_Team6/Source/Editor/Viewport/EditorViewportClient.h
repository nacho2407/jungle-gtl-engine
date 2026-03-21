#pragma once

#include "Render/Common/RenderTypes.h"

#include "Viewport/CursorOverlayState.h"
#include <string>
#include "Core/RayTypes.h"
#include "Core/CollisionTypes.h"
#include "Core/Common.h"

class UWorld;
class UCameraComponent;
class UGizmoComponent;
class FEditorSettings;
class FWindowsWindow;

using namespace common::structs;

class FEditorViewportClient
{
public:
	void Initialize(FWindowsWindow* InWindow);
	void SetWorld(UWorld* InWorld) { World = InWorld; }
	void SetCamera(UCameraComponent* InCamera) { Camera = InCamera; }
	void SetGizmo(UGizmoComponent* InGizmo) { Gizmo = InGizmo; }
	void SetSettings(const FEditorSettings* InSettings) { Settings = InSettings; }
	UGizmoComponent* GetGizmo() { return Gizmo; }
	void SetViewportSize(float InWidth, float InHeight);

	void Tick(float DeltaTime);

	const FCursorOverlayState& GetCursorOverlayState() const { return CursorOverlayState; }
	FViewOutput& GetViewOutput() { return ViewOutput;  }

private:
	void ClearViewOutput();
	void TickInput(float DeltaTime);
	void TickInteraction(float DeltaTime);
	void TickCursorOverlay(float DeltaTime);

	void HandleDragStart(const FRay& Ray);

private:
	FWindowsWindow* Window = nullptr;
	UWorld* World = nullptr;
	UCameraComponent* Camera = nullptr;
	UGizmoComponent* Gizmo = nullptr;
	const FEditorSettings* Settings = nullptr;

	float WindowWidth = 1920.f;
	float WindowHeight = 1080.f;

	bool bIsCursorVisible = true;

	FCursorOverlayState CursorOverlayState;
	FViewOutput ViewOutput;
};
