#pragma once

#include "Render/Common/RenderTypes.h"
#include "Viewport/CursorOverlayState.h"
#include "Core/RayTypes.h"
#include "Core/CollisionTypes.h"
#include "Editor/Viewport/ViewportCamera.h"
#include "Editor/Viewport/ViewportNavigationController.h"

class UWorld;
class UGizmoComponent;
class FEditorSettings;
class FWindowsWindow;
class FSelectionManager;

class FEditorViewportClient
{
public:
	void Initialize(FWindowsWindow* InWindow);
	void SetWorld(UWorld* InWorld) { World = InWorld; }
	void SetGizmo(UGizmoComponent* InGizmo) { Gizmo = InGizmo; }
	void SetSettings(const FEditorSettings* InSettings) { Settings = InSettings; }
	void SetSelectionManager(FSelectionManager* InSelectionManager);
	UGizmoComponent* GetGizmo() { return Gizmo; }
	void SetViewportSize(float InWidth, float InHeight);

	// Camera lifecycle
	void CreateCamera();
	void DestroyCamera();
	void ResetCamera();
	FViewportCamera* GetCamera() { return bHasCamera ? &Camera : nullptr; }
	const FViewportCamera* GetCamera() const { return bHasCamera ? &Camera : nullptr; }

	void Tick(float DeltaTime);

	const FCursorOverlayState& GetCursorOverlayState() const { return CursorOverlayState; }

private:
	void TickInput(float DeltaTime);
	void TickInteraction(float DeltaTime);
	void TickCursorOverlay(float DeltaTime);

	void HandleDragStart(const FRay& Ray);

	FVector ResolveOrbitPivot() const;

private:
	FWindowsWindow* Window = nullptr;
	UWorld* World = nullptr;
	UGizmoComponent* Gizmo = nullptr;
	const FEditorSettings* Settings = nullptr;
	FSelectionManager* SelectionManager = nullptr;

	FViewportCamera Camera;
	FViewportNavigationController NavigationController;
	bool bHasCamera = false;

	float WindowWidth = 1920.f;
	float WindowHeight = 1080.f;

	bool bIsCursorVisible = true;

	// Input state bridge for current singleton InputSystem
	bool bRightMouseRotating = false;
	bool bMiddleMousePanning = false;
	bool bAltLeftMouseOrbiting = false;

	bool bFirstMouseMoveAfterRotateStart = false;
	bool bFirstMouseMoveAfterPanStart = false;
	bool bFirstMouseMoveAfterOrbitStart = false;

	FCursorOverlayState CursorOverlayState;
};