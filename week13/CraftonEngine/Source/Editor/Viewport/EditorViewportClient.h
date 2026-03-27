#pragma once

#include "Viewport/ViewportClient.h"
#include "Render/Common/RenderTypes.h"

#include "Viewport/CursorOverlayState.h"
#include <string>
#include "Core/RayTypes.h"
#include "Core/CollisionTypes.h"
class UWorld;
class UCameraComponent;
class UGizmoComponent;
class FEditorSettings;
class FWindowsWindow;
class FSelectionManager;
class FViewport;

class FEditorViewportClient : public FViewportClient
{
public:
	void Initialize(FWindowsWindow* InWindow);
	void SetWorld(UWorld* InWorld) { World = InWorld; }
	void SetGizmo(UGizmoComponent* InGizmo) { Gizmo = InGizmo; }
	void SetSettings(const FEditorSettings* InSettings) { Settings = InSettings; }
	void SetSelectionManager(FSelectionManager* InSelectionManager) { SelectionManager = InSelectionManager; }
	UGizmoComponent* GetGizmo() { return Gizmo; }
	void SetViewportSize(float InWidth, float InHeight);

	// Camera lifecycle
	void CreateCamera();
	void DestroyCamera();
	void ResetCamera();
	UCameraComponent* GetCamera() const { return Camera; }

	void Tick(float DeltaTime);

	// 활성 상태 — 활성 뷰포트만 입력 처리
	void SetActive(bool bInActive) { bIsActive = bInActive; }
	bool IsActive() const { return bIsActive; }

	const FCursorOverlayState& GetCursorOverlayState() const { return CursorOverlayState; }

	// FViewport 소유
	void SetViewport(FViewport* InViewport) { Viewport = InViewport; }
	FViewport* GetViewport() const { return Viewport; }

private:
	void TickInput(float DeltaTime);
	void TickInteraction(float DeltaTime);
	void TickCursorOverlay(float DeltaTime);

	void HandleDragStart(const FRay& Ray);

private:
	FViewport* Viewport = nullptr;
	FWindowsWindow* Window = nullptr;
	UWorld* World = nullptr;
	UCameraComponent* Camera = nullptr;
	UGizmoComponent* Gizmo = nullptr;
	const FEditorSettings* Settings = nullptr;
	FSelectionManager* SelectionManager = nullptr;

	float WindowWidth = 1920.f;
	float WindowHeight = 1080.f;

	bool bIsActive = false;
	bool bIsCursorVisible = true;

	FCursorOverlayState CursorOverlayState;
};
