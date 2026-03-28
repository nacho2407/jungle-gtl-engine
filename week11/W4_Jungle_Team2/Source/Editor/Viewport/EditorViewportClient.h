#pragma once

#include "Render/Common/RenderTypes.h"

#include <string>
#include "Core/RayTypes.h"
#include "Core/CollisionTypes.h"
#include "Runtime/ViewportClient.h"

enum EEditorViewportType
{
	EVT_Perspective = 0,		// Perspective
	EVT_OrthoXY = 1,			// Top
	EVT_OrthoXZ = 2,			// Right
	EVT_OrthoYZ = 3,			// Back
	EVT_OrthoNegativeXY = 4,	// Bottom
	EVT_OrthoNegativeXZ = 5,	// Left
	EVT_OrthoNegativeYZ = 6,	// Front

	EVT_OrthoTop = EVT_OrthoXY,				// TOP
	EVT_OrthoLeft = EVT_OrthoXZ,			// Left
	EVT_OrthoFront = EVT_OrthoNegativeYZ,	// Front
	EVT_OrthoBack = EVT_OrthoYZ,			// Back
	EVT_OrthoBottom = EVT_OrthoNegativeXY,	// Bottom
	EVT_OrthoRight = EVT_OrthoNegativeXZ,	// Right
	LVT_MAX = 7,
};


class UWorld;
class UGizmoComponent;
class FEditorSettings;
class FWindowsWindow;
class FSelectionManager;
class FSceneViewport;
struct FEditorViewportState;

/*
* 뷰포트별 카메라 / 뷰모드 / 입력 / 피킹 / 기즈모
* BuildSceneView
* orthographic / perspective 분기
* Gizmo axis visibility 분기
*/

class FEditorViewportClient : public FViewportClient
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

	// Renderer 에서 사용하는 Proj 정보가 담긴 SceneView를 생성하는 함수
	void BuildSceneView(FSceneView& OutView) const override;

	// Key 입력 대응 함수
	bool OnMouseMove(const FViewportMouseEvent& Ev) override;
	bool OnMouseButtonDown(const FViewportMouseEvent& Ev) override;
	bool OnMouseButtonUp(const FViewportMouseEvent& Ev) override;
	bool OnMouseWheel(float Delta) override;
	bool OnKeyDown(uint32 Key) override;
	bool OnKeyUp(uint32 Key) override;

	// Get Set
	EEditorViewportType GetViewportType() const { return ViewportType; }
	void SetViewportType(EEditorViewportType InType) { ViewportType = InType; }

	void SetViewport(FSceneViewport* InViewport) { Viewport = InViewport; }
	void SetState(FEditorViewportState* InState) { State = InState; }

	// ViewportType에 맞게 카메라 초기화.
	void ApplyCameraMode();

private:
	void TickInput(float DeltaTime);
	void TickInteraction(float DeltaTime);
	void HandleDragStart(const FRay& Ray);

	FVector ResolveOrbitPivot() const;

private:
	// Viewport <-> ViewportClient는 상호참조(상위 객체 소유권)
	FWindowsWindow* Window = nullptr;

	FSceneViewport* Viewport = nullptr;

	EEditorViewportType  ViewportType = EVT_Perspective;  // 값 멤버로 직접 보유
	FEditorViewportState* State = nullptr;

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