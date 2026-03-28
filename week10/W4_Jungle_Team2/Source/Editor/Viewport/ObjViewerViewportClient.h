#pragma once
#pragma once

#include "Render/Common/RenderTypes.h"

#include "Viewport/CursorOverlayState.h"
#include <string>
#include "Core/RayTypes.h"
#include "Core/CollisionTypes.h"

class UWorld;
class UCameraComponent;
class FObjViewerSettings;
class FWindowsWindow;
class FSelectionManager;

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

	// 카메라 조작감 개선
	void ClampCameraPosition();
	void ClampCameraPanToObject();
	float GetModelRadius();

	UCameraComponent* GetCamera() const { return Camera; }

	void Tick(float DeltaTime);

	const FCursorOverlayState& GetCursorOverlayState() const { return CursorOverlayState; }
	
private:
	void TickInput(float DeltaTime);
	void TickInteraction(float DeltaTime);
	void TickCursorOverlay(float DeltaTime);

	void HandleDragStart(const FRay& Ray);

private:
	FWindowsWindow* Window = nullptr;
	UWorld* World = nullptr;
	UCameraComponent* Camera = nullptr;
	const FObjViewerSettings* Settings = nullptr;
	FSelectionManager* SelectionManager = nullptr;

	float WindowWidth = 1920.f;
	float WindowHeight = 1080.f;
	
	bool bIsCursorVisible = true;
	
	// Viewer 전용 중심점 기준 회전 조작에 사용
	bool bIsOrbiting = false;
	FVector OrbitPivot = FVector(0.0f, 0.0f, 0.0f);
	float OrbitDistance = 50.0f;

	FCursorOverlayState CursorOverlayState;
};
