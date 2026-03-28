#pragma once

#include "Render/Common/RenderTypes.h"

#include <string>
#include "Core/RayTypes.h"
#include "Core/CollisionTypes.h"
#include "Runtime/ViewportClient.h"


class UWorld;
class UCameraComponent;
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
	void SetSelectionManager(FSelectionManager* InSelectionManager) { SelectionManager = InSelectionManager; }
	UGizmoComponent* GetGizmo() { return Gizmo; }
	void SetViewportSize(float InWidth, float InHeight);

	// Camera lifecycle
	void CreateCamera();
	void DestroyCamera();
	void ResetCamera();
	UCameraComponent* GetCamera() const { return Camera; }

	void Tick(float DeltaTime);

	// Renderer 에서 사용하는 Proj 정보가 담긴 SceneView를 생성하는 함수
	void BuildSceneView(FSceneView& OutView) const override;

private:
	void TickInput(float DeltaTime);
	void TickInteraction(float DeltaTime);
	void HandleDragStart(const FRay& Ray);

private:
	// Viewport와 ViewportClient는 상호참조(상위 객체 소유권)
	FWindowsWindow* Window = nullptr;

	FSceneViewport* Viewport = nullptr;
	FEditorViewportState* State = nullptr;

	UWorld* World = nullptr;
	UCameraComponent* Camera = nullptr;
	UGizmoComponent* Gizmo = nullptr;
	const FEditorSettings* Settings = nullptr;
	FSelectionManager* SelectionManager = nullptr;

	float WindowWidth = 1920.f;
	float WindowHeight = 1080.f;

	bool bIsCursorVisible = true;
};
