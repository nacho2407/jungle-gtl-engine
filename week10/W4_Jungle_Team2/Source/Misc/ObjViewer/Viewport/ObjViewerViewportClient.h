#pragma once
#include "Render/Common/RenderTypes.h"

#include "Viewport/CursorOverlayState.h"
#include <string>
#include "Core/RayTypes.h"
#include "Core/CollisionTypes.h"


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

    FVector ResetStartOrbitPivot;
    FVector ResetTargetOrbitPivot;
    float ResetStartOrbitDistance = 0.0f;
    float ResetTargetOrbitDistance = 0.0f;
};

class FObjViewerViewportClient
{
public:
	void Initialize(FWindowsWindow* InWindow);
	void SetWorld(UWorld* InWorld) { World = InWorld; }
	void SetSettings(const FObjViewerSettings* InSettings) { Settings = InSettings; }
	void SetSelectionManager(FSelectionManager* InSelectionManager) { SelectionManager = InSelectionManager; }
	void SetViewportSize(float InWidth, float InHeight);

	// Camera lifecycle
	void CreateCamera();
	void DestroyCamera();
	void ResetCamera();
	void ResetCameraSmoothly();
	void SaveCameraPosition();

	// 카메라 조작감 개선
	void ClampCameraPosition();
	void ClampCameraPanToObject();
	ObjViewerModelInfo GetModelInfo();
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

	float ViewportX = 0.0f;
    float ViewportY = 0.0f;
	float WindowWidth = 1920.f;
	float WindowHeight = 1080.f;
	
	bool bIsCursorVisible = true;
	bool bSavedCameraPosition = false;
	FVector SavedCameraLocation;
	FQuat SavedCameraRotation;
	FVector SavedOrbitPivot;
	float SavedOrbitDistance = 10.0f;
	
	FCameraGUIParameters CameraGUIParams;
	
	// Viewer 전용 중심점 기준 회전 조작에 사용
	bool bIsOrbiting = false;
	FVector OrbitPivot = FVector(0.0f, 0.0f, 0.0f);
	float OrbitDistance = 10.0f;

	FCursorOverlayState CursorOverlayState;
};
