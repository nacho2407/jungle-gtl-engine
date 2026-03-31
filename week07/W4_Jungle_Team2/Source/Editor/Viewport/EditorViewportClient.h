#pragma once

#include "Render/Common/RenderTypes.h"
#include "Core/RayTypes.h"
#include "Core/CollisionTypes.h"
#include "Runtime/ViewportClient.h"
#include "Viewport/ViewportNavigationController.h"

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
class FViewportNavigationController;
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
	float GetMoveSpeed() const { return NavigationController.GetMoveSpeed(); }
	void SetMoveSpeed(float InSpeed) { NavigationController.SetMoveSpeed(InSpeed); }
	void FocusSelection() { FocusPrimarySelection(); }

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

	FSceneViewport* GetViewport() { return Viewport; }
	const FSceneViewport* GetViewport() const { return Viewport; }
	void SetViewport(FSceneViewport* InViewport) { Viewport = InViewport; }

	FEditorViewportState* GetViewportState() { return State; }
	const FEditorViewportState* GetViewportState() const { return State; }
	void SetState(FEditorViewportState* InState) { State = InState; }

	// ViewportType에 맞게 카메라 초기화.
	void ApplyCameraMode();
	
	// 현재 EditorViewportClient가 참조하는 Viewport 에서 독점 조작이 진행 중인지 반환합니다.
	bool IsActiveOperation() const
	{
		return bRightMouseRotating || bRightMousePanning
			|| bMiddleMousePanning
			|| bAltRightMouseDollying;
	}
	bool IsBoxSelecting() const { return bBoxSelecting; }
	POINT GetBoxSelectStart() const { return BoxSelectStart; }
	POINT GetBoxSelectEnd() const { return BoxSelectEnd; }
private:
	void TickInput(float DeltaTime);
	void TickInteraction(float DeltaTime);
	void HandleDragStart(const FRay& Ray);
	void HandleBoxSelection();
	bool TryProjectWorldToViewport(const FVector& WorldPos, float& OutViewportX, float& OutViewportY, float& OutDepth) const;
	void FocusPrimarySelection();
	void DeleteSelectedActors();
	void SelectAllActors();

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
	bool bMouseConstrained = false;

	// Input state bridge for current singleton InputSystem
	bool bRightMouseRotating = false;
	bool bRightMousePanning  = false;   // 직교 뷰: 우클릭 드래그 = 팬
	bool bMiddleMousePanning = false;
	bool bAltRightMouseDollying = false;

	bool bBoxSelecting = false;
	POINT BoxSelectStart = { 0, 0 };
	POINT BoxSelectEnd = { 0, 0 };

	bool bFirstMouseMoveAfterRotateStart   = false;
	bool bFirstMouseMoveAfterRightPanStart = false;
	bool bFirstMouseMoveAfterPanStart      = false;
	bool bFirstMouseMoveAfterDollyStart    = false;
};
