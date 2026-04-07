#pragma once
#include "Render/Common/RenderTypes.h"

#include "Viewport/CursorOverlayState.h"
#include <string>
#include "Engine/Geometry/Ray.h"
#include "Core/CollisionTypes.h"

// Editor의 네비게이션 컨트롤러를 포함합니다.
#include "Editor/Viewport/ViewportNavigationController.h" 

class FViewportCamera;
class UWorld;
class FObjViewerSettings;
class FWindowsWindow;
class FSelectionManager;

struct ObjViewerModelInfo
{
	FVector ModelCenter = FVector(0.0f, 0.0f, 0.0f);
	float ModelRadius = 2.0f;
};

// 카메라 초기화 애니메이션을 위한 상태 변수
struct FCameraGUIParameters
{
	bool bIsResettingCamera = false;
    float ResetCameraProgress = 0.0f;
    float ResetCameraSpeed = 2.5f;

    FVector ResetStartLocation;
    FVector ResetTargetLocation;
    FQuat ResetStartRotation;
    FQuat ResetTargetRotation;
};

class FObjViewerViewportClient
{
public:
	void Initialize(FWindowsWindow* InWindow);
	void SetWorld(UWorld* InWorld) { World = InWorld; }
	void SetSettings(const FObjViewerSettings* InSettings) { Settings = InSettings; }
	void SetSelectionManager(FSelectionManager* InSelectionManager) 
	{ 
		SelectionManager = InSelectionManager; 
		NavigationController.SetSelectionManager(InSelectionManager); 
	}
	void SetViewportSize(float InWidth, float InHeight);

	// Camera lifecycle
	void CreateCamera();
	void DestroyCamera();
	void ResetCamera();
	void ResetCameraSmoothly();
	void SaveCameraPosition();

	FViewportCamera* GetCamera() const { return Camera; }

	// SetViewportSize를 대체
    void SetViewportRect(float InX, float InY, float InWidth, float InHeight);
    float GetViewportX() const { return ViewportX; }
    float GetViewportY() const { return ViewportY; }
    float GetViewportWidth() const { return WindowWidth; }
    float GetViewportHeight() const { return WindowHeight; }

	void Tick(float DeltaTime);

	const FCursorOverlayState& GetCursorOverlayState() const { return CursorOverlayState; }
	
private:
	void TickInput(float DeltaTime);
	void TickInteraction(float DeltaTime);
	void TickCursorOverlay(float DeltaTime);
    void TickCameraReset(float DeltaTime);

	void HandleDragStart(const FRay& Ray);

private:
	FWindowsWindow* Window = nullptr;
	UWorld* World = nullptr;
	FViewportCamera* Camera = nullptr;
	const FObjViewerSettings* Settings = nullptr;
	FSelectionManager* SelectionManager = nullptr;

	// 기존 수동 짐벌락/팬 연산 대신 Editor의 컨트롤러 사용
	FViewportNavigationController NavigationController;

	float ViewportX = 0.0f;
    float ViewportY = 0.0f;
	float WindowWidth = 1920.f;
	float WindowHeight = 1080.f;
	
	bool bIsCursorVisible = true;
	bool bSavedCameraPosition = false;
	FVector SavedCameraLocation;
	FQuat SavedCameraRotation;
	
	FCameraGUIParameters CameraGUIParams;

	// 입력 상태 추적 변수들 (EditorViewportClient에서 차용)
	bool bRightMouseRotating = false;		// 우클릭 드래그 = 회전
	bool bRightMousePanning  = false;		// 직교 뷰 우클릭 = 팬
	bool bMiddleMousePanning = false;		// 중클릭 드래그 = 팬
	bool bAltRightMouseDollying = false;	// Alt + 우클릭 = Dolly(줌)

	bool bFirstMouseMoveAfterRotateStart   = false;
	bool bFirstMouseMoveAfterRightPanStart = false;
	bool bFirstMouseMoveAfterPanStart      = false;
	bool bFirstMouseMoveAfterDollyStart    = false;

	FCursorOverlayState CursorOverlayState;
};