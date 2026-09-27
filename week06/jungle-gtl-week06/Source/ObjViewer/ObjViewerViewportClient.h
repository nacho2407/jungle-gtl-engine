#pragma once

#include "Viewport/ViewportClient.h"
#include "Math/Vector.h"

class UCameraComponent;
class FWindowsWindow;
class FViewport;

// ObjViewer용 간이 뷰포트 클라이언트 — 마우스 오빗/줌/팬
class FObjViewerViewportClient : public FViewportClient
{
public:
	void Initialize(FWindowsWindow* InWindow);
	void Release();

	// Camera lifecycle
	void CreateCamera();
	void DestroyCamera();
	void ResetCamera();
	UCameraComponent* GetCamera() const { return Camera; }

	// Viewport
	void SetViewport(FViewport* InViewport) { Viewport = InViewport; }
	FViewport* GetViewport() const { return Viewport; }

	void Tick(float DeltaTime);

	// 뷰포트 영역 설정 (ImGui 패널에서 호출)
	void SetViewportRect(float X, float Y, float Width, float Height);

	// ImDrawList에 SRV를 그려주는 헬퍼
	void RenderViewportImage();

private:
	void TickInput(float DeltaTime);

private:
	FViewport* Viewport = nullptr;
	FWindowsWindow* Window = nullptr;
	UCameraComponent* Camera = nullptr;

	// 오빗 파라미터
	FVector OrbitTarget = FVector(0, 0, 0);
	float OrbitDistance = 5.0f;
	float OrbitYaw = 0.0f;		// degrees
	float OrbitPitch = 30.0f;	// degrees

	// 뷰포트 스크린 영역
	float ViewportX = 0.0f;
	float ViewportY = 0.0f;
	float ViewportWidth = 800.0f;
	float ViewportHeight = 600.0f;
};
