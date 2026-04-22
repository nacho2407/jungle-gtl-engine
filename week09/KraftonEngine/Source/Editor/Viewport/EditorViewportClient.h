#pragma once

#include "Viewport/ViewportClient.h"
#include "Render/Execute/Context/Scene/ViewTypes.h"

#include "UI/SWindow.h"
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
class FOverlayStatSystem;

enum class EEditorViewportPlayState : uint8
{
	Stopped,
	Playing,
	Paused,
};

class FEditorViewportClient : public FViewportClient
{
public:
	void Initialize(FWindowsWindow* InWindow);
	void SetOverlayStatSystem(FOverlayStatSystem* InOverlayStatSystem) { OverlayStatSystem = InOverlayStatSystem; }
	// World�� �� �̻� �������� �ʴ´� ? GetWorld()�� GEngine->GetWorld()�� �����Ͽ�
	// ActiveWorldHandle�� �����Ƿ� PIE ��ȯ �� �ڵ����� �ùٸ� ���带 ��ȯ�Ѵ�.
	UWorld* GetWorld() const;
	void SetGizmo(UGizmoComponent* InGizmo) { Gizmo = InGizmo; }
	void SetSettings(const FEditorSettings* InSettings) { Settings = InSettings; }
	void SetSelectionManager(FSelectionManager* InSelectionManager) { SelectionManager = InSelectionManager; }
	UGizmoComponent* GetGizmo() { return Gizmo; }

	// ����Ʈ�� ���� �ɼ�
	FViewportRenderOptions& GetRenderOptions() { return RenderOptions; }
	const FViewportRenderOptions& GetRenderOptions() const { return RenderOptions; }

	// ����Ʈ Ÿ�� ��ȯ (Perspective / Ortho ����)
	void SetViewportType(ELevelViewportType NewType);
	void SetViewportSize(float InWidth, float InHeight);

	// Camera lifecycle
	void CreateCamera();
	void DestroyCamera();
	void ResetCamera();
	UCameraComponent* GetCamera() const { return Camera; }

	void Tick(float DeltaTime);

	// Ȱ�� ���� ? Ȱ�� ����Ʈ�� �Է� ó��
	void SetActive(bool bInActive) { bIsActive = bInActive; }
	bool IsActive() const { return bIsActive; }

	void SetPlayState(EEditorViewportPlayState InPlayState) { PlayState = InPlayState; }
	EEditorViewportPlayState GetPlayState() const { return PlayState; }
	void SetPaneToolbarHeight(float InHeight) { PaneToolbarHeight = InHeight; }
	float GetPaneToolbarHeight() const { return PaneToolbarHeight; }

	// FViewport ����
	void SetViewport(FViewport* InViewport) { Viewport = InViewport; }
	FViewport* GetViewport() const override { return Viewport; }

	// SWindow ���̾ƿ� ���� ? SSplitter ���� ���
	void SetLayoutWindow(SWindow* InWindow) { LayoutWindow = InWindow; }
	SWindow* GetLayoutWindow() const { return LayoutWindow; }

	// SWindow Rect �� ViewportScreenRect ���� + FViewport �������� ��û
	void UpdateLayoutRect();

	// ImDrawList�� �ڽ��� SRV�� SWindow Rect ��ġ�� ����
	void RenderViewportImage();
	void RenderViewportBorder();

private:
	void TickEditorShortcuts();
	void TickInput(float DeltaTime);
	void TickInteraction(float DeltaTime);
	void HandleDragStart(const FRay& Ray); //��ŷ ����

private:
	FViewport* Viewport = nullptr;
	SWindow* LayoutWindow = nullptr;
	FWindowsWindow* Window = nullptr;
	FOverlayStatSystem* OverlayStatSystem = nullptr;
	UCameraComponent* Camera = nullptr;
	UGizmoComponent* Gizmo = nullptr;
	const FEditorSettings* Settings = nullptr;
	FSelectionManager* SelectionManager = nullptr;
	FViewportRenderOptions RenderOptions;

	float WindowWidth = 1920.f;
	float WindowHeight = 1080.f;

	bool bIsActive = false;
	EEditorViewportPlayState PlayState = EEditorViewportPlayState::Stopped;
	float PaneToolbarHeight = 0.0f;
	// ����Ʈ ���� ���� ����(���� ����)
	FRect ViewportScreenRect;
	// ����Ʈ ������ ����(���� ���� ��ü �г�)
	FRect ViewportFrameRect;
};
