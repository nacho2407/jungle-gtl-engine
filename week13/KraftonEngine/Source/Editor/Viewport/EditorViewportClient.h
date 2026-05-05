#pragma once

#include "Viewport/ViewportClient.h"
#include "Render/Types/RenderTypes.h"
#include "Render/Types/ViewTypes.h"
#include "Editor/Viewport/ViewportCameraTransform.h"

#include "UI/SWindow.h"
#include <string>
#include "Core/RayTypes.h"
#include "Core/CollisionTypes.h"
#include "Math/Rotator.h"
#include "imgui.h"

class UWorld;
class UGizmoComponent;
class ULightComponentBase;
class FEditorSettings;
class FWindowsWindow;
class FSelectionManager;
class FViewport;
class FOverlayStatSystem;
struct FMinimalViewInfo;

class FEditorViewportClient : public FViewportClient
{
public:
	void Initialize(FWindowsWindow* InWindow);
	void SetOverlayStatSystem(FOverlayStatSystem* InOverlayStatSystem) { OverlayStatSystem = InOverlayStatSystem; }
	// World는 더 이상 저장하지 않는다 — GetWorld()는 GEngine->GetWorld()를 경유하여
	// ActiveWorldHandle을 따르므로 PIE 전환 시 자동으로 올바른 월드를 반환한다.
	UWorld* GetWorld() const;
	void SetGizmo(UGizmoComponent* InGizmo) { Gizmo = InGizmo; }
	void SetSettings(const FEditorSettings* InSettings) { Settings = InSettings; }
	void SetSelectionManager(FSelectionManager* InSelectionManager) { SelectionManager = InSelectionManager; }
	UGizmoComponent* GetGizmo() { return Gizmo; }

	// 뷰포트별 렌더 옵션
	FViewportRenderOptions& GetRenderOptions() { return RenderOptions; }
	const FViewportRenderOptions& GetRenderOptions() const { return RenderOptions; }

	// 뷰포트 타입 전환 (Perspective / Ortho 방향)
	void SetViewportType(ELevelViewportType NewType);
	void SetViewportSize(float InWidth, float InHeight);

	// 뷰포트 리사이즈 통지 — 카메라 AspectRatio 갱신.
	// 렌더 파이프라인이 컴포넌트를 직접 안 만지도록 책임을 viewport client 로 이동.
	void NotifyViewportResized(int32 NewWidth, int32 NewHeight);

	// Camera lifecycle (잔여 정리: ViewTransform 이 SoT, 컴포넌트 mirror 제거됨).
	// CreateCamera/DestroyCamera 는 더 이상 필요 없으나 호출자 기존 흐름 보존 위해 no-op 유지.
	void CreateCamera() {}
	void DestroyCamera() {}
	void ResetCamera();

	// 카메라 POV 통화 산출 — 외부에 노출되는 카메라 데이터 API.
	void GetCameraView(FMinimalViewInfo& OutPOV) const;

	// Editor 카메라 데이터 SoT. UI 위젯이 이 값을 직접 편집하고 NotifyViewTransformChanged 호출.
	FViewportCameraTransform& GetViewTransform() { return ViewTransform; }
	const FViewportCameraTransform& GetViewTransform() const { return ViewTransform; }

	// 외부에서 ViewTransform 을 mutate 한 후 호출 — Active viewport 면 World 의 EditorActivePOV 갱신.
	void NotifyViewTransformChanged();

	void Tick(float DeltaTime);

	// 활성 상태 — 활성 뷰포트만 입력 처리
	void SetActive(bool bInActive) { bIsActive = bInActive; }
	bool IsActive() const { return bIsActive; }

	// FViewport 소유
	void SetViewport(FViewport* InViewport) { Viewport = InViewport; }
	FViewport* GetViewport() const { return Viewport; }

	// 뷰포트 스크린 좌표 (ImGui screen space)
	const FRect& GetViewportScreenRect() const { return ViewportScreenRect; }

	// 마우스가 뷰포트 안에 있으면 뷰포트 로컬 좌표 반환 (시각화용)
	bool GetCursorViewportPosition(uint32& OutX, uint32& OutY) const;

	// SWindow 레이아웃 연결 — SSplitter 리프 노드
	void SetLayoutWindow(SWindow* InWindow) { LayoutWindow = InWindow; }
	SWindow* GetLayoutWindow() const { return LayoutWindow; }

	// SWindow Rect → ViewportScreenRect 갱신 + FViewport 리사이즈 요청
	void UpdateLayoutRect();

	// ImDrawList에 자신의 SRV를 SWindow Rect 위치에 렌더 (활성 테두리 포함)
	void RenderViewportImage(bool bIsActiveViewport);

	// Light View Override — 라이트 시점으로 카메라 오버라이드
	void SetLightViewOverride(ULightComponentBase* Light);
	void ClearLightViewOverride();
	bool IsViewingFromLight() const { return LightViewOverride != nullptr; }
	ULightComponentBase* GetLightViewOverride() const { return LightViewOverride; }

	// PointLight face index (0~5: +X,-X,+Y,-Y,+Z,-Z)
	int32 GetPointLightFaceIndex() const { return PointLightFaceIndex; }
	void SetPointLightFaceIndex(int32 Index) { PointLightFaceIndex = (Index < 0) ? 0 : (Index > 5) ? 5 : Index; }

private:
	void TickEditorShortcuts();
	void TickInput(float DeltaTime);
	void TickInteraction(float DeltaTime);
	void HandleDragStart(const FRay& Ray); //픽킹 시작
	void SyncCameraSmoothingTarget();
	void ApplySmoothedCameraLocation(float DeltaTime);

	// 잔여 정리: ViewTransform 만 SoT — Camera 컴포넌트 mirror 자체 제거됨.
	// Active viewport 의 POV 를 World 의 EditorActivePOV 슬롯에 푸시 (LOD/render 용).
	void PushPOVToWorld();

private:
	FViewport* Viewport = nullptr;
	SWindow* LayoutWindow = nullptr;
	FWindowsWindow* Window = nullptr;
	FOverlayStatSystem* OverlayStatSystem = nullptr;
	UGizmoComponent* Gizmo = nullptr;
	const FEditorSettings* Settings = nullptr;
	FSelectionManager* SelectionManager = nullptr;
	FViewportRenderOptions RenderOptions;
	ULightComponentBase* LightViewOverride = nullptr;
	int32 PointLightFaceIndex = 0;

	// Editor 카메라 데이터 SoT
	FViewportCameraTransform ViewTransform;

	float WindowWidth = 1920.f;
	float WindowHeight = 1080.f;

	bool bIsActive = false;
	// 뷰포트 슬롯의 스크린 좌표 (ImGui screen space = 윈도우 클라이언트 좌표)
	FRect ViewportScreenRect;

	// Marquee Selection (영역 드래그 선택)
	bool bIsMarqueeSelecting = false;
	FVector MarqueeStartPos;
	FVector MarqueeCurrentPos;

	// Camera Focus Animation
	bool bIsFocusAnimating = false;
	FVector FocusStartLoc;
	FRotator FocusStartRot;
	FVector FocusEndLoc;
	FRotator FocusEndRot;
	float FocusAnimTimer = 0.0f;
	const float FocusAnimDuration = 0.5f; // 0.5초 동안 이동

	// Camera Smoothing
	FVector TargetLocation;
	bool bTargetLocationInitialized = false;
	FVector LastAppliedCameraLocation;
	bool bLastAppliedCameraLocationInitialized = false;
	const float SmoothLocationSpeed = 10.0f;
};
