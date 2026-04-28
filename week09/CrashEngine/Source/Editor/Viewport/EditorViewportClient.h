// 에디터 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include <memory>
#include <string>

#include "Core/RayTypes.h"
#include "Core/CollisionTypes.h"

#include "Input/InputTypes.h"

#include "UI/SWindow.h"

#include "Viewport/ViewportClient.h"

#include "Render/Execute/Context/Scene/ViewTypes.h"
#include "Math/Rotator.h"
#include "Editor/Input/EditorViewportInputController.h"

class UWorld;
class AActor;
class UCameraComponent;
class UGizmoComponent;
class FEditorSettings;
class FWindowsWindow;
class FSelectionManager;
class FViewport;
class FOverlayStatSystem;

// EEditorViewportPlayState는 에디터 처리에서 사용할 선택지를 정의합니다.
enum class EEditorViewportPlayState : uint8
{
    Stopped,
    Playing,
    Paused,
};

/**
 * @brief FEditorViewportClient가 한 프레임 동안 받은 입력 event를 저장하는 구조체
 */
struct FEditorViewportFrameInput
{
    bool KeyDown[256] = {};
    bool KeyPressed[256] = {};
    bool KeyReleased[256] = {};
    bool KeyRepeated[256] = {};

    POINT MouseLocalPos = { 0, 0 };
    POINT MouseClientPos = { 0, 0 };
    POINT MouseScreenPos = { 0, 0 };

    POINT MouseAxisDelta = { 0, 0 };
    float MouseWheelNotches = 0.0f;

    bool bLeftPressed = false;
    bool bLeftDown = false;
    bool bLeftReleased = false;

    bool bRightPressed = false;
    bool bRightDown = false;
    bool bRightReleased = false;

    bool bMiddlePressed = false;
    bool bMiddleDown = false;
    bool bMiddleReleased = false;

    FInputModifiers Modifiers;
};

// FEditorViewportClient는 카메라와 화면 출력에 필요한 상태를 다룹니다.
class FEditorViewportClient : public FViewportClient
{
public:
    void Initialize(FWindowsWindow* InWindow);
    void SetOverlayStatSystem(FOverlayStatSystem* InOverlayStatSystem) { OverlayStatSystem = InOverlayStatSystem; }
    FOverlayStatSystem* GetOverlayStatSystem() const { return OverlayStatSystem; }
    UWorld* GetWorld() const;
    void SetGizmo(UGizmoComponent* InGizmo) { Gizmo = InGizmo; }
    void SetSettings(const FEditorSettings* InSettings) { Settings = InSettings; }
    const FEditorSettings* GetSettings() const { return Settings; }
    void SetSelectionManager(FSelectionManager* InSelectionManager) { SelectionManager = InSelectionManager; }
    UGizmoComponent* GetGizmo() { return Gizmo; }

    FViewportRenderOptions& GetRenderOptions() { return RenderOptions; }
    const FViewportRenderOptions& GetRenderOptions() const { return RenderOptions; }

    void SetViewportType(ELevelViewportType NewType);
    void SetViewportSize(float InWidth, float InHeight);

    // Camera lifecycle
    void CreateCamera();
    void DestroyCamera();
    void ResetCamera();
    UCameraComponent* GetCamera() const { return Camera; }

    void Tick(float DeltaTime);
    void PilotSelectedActor(AActor* Actor);
    void StopPilotingActor();
    void UpdateViewFromPilotedActor();
    void ApplyViewToPilotedActor();

    bool IsPilotingActor() const { return bIsPilotingActor && PilotedActor != nullptr; }
    AActor* GetPilotedActor() const { return PilotedActor; }
    FString GetActorDisplayName(const AActor* Actor) const;
    FString GetPilotedActorDisplayName() const;
    FString GetPilotOverlayText() const;
    FString GetPilotHintText() const;
    AActor* PickActorAtScreenPoint(float ScreenX, float ScreenY) const;

    void SetActive(bool bInActive) { bIsActive = bInActive; }
    bool IsActive() const { return bIsActive; }

    void SetPlayState(EEditorViewportPlayState InPlayState) { PlayState = InPlayState; }
    EEditorViewportPlayState GetPlayState() const { return PlayState; }
    void SetPaneToolbarHeight(float InHeight) { PaneToolbarHeight = InHeight; }
    float GetPaneToolbarHeight() const { return PaneToolbarHeight; }

    void SetViewport(FViewport* InViewport) { Viewport = InViewport; }
    FViewport* GetViewport() const override { return Viewport; }

    void SetLayoutWindow(SWindow* InWindow) { LayoutWindow = InWindow; }
    SWindow* GetLayoutWindow() const { return LayoutWindow; }

    void UpdateLayoutRect();

    void RenderViewportImage();
    void RenderViewportBorder();

	bool InputKey(const FViewportKeyEvent& Event) override;
    bool InputAxis(const FViewportAxisEvent& Event) override;
    bool InputPointer(const FViewportPointerEvent& Event) override;

	const FRect& GetViewportScreenRect() const { return ViewportScreenRect; }
    const FRect& GetViewportFrameRect() const { return ViewportFrameRect; }

    void BeginInputFrame();
    const FEditorViewportFrameInput& GetCurrentInput() const { return CurrentInput; }

	FEditorViewportInputController* GetInputController() const
    {
        return InputController.get();
    }

	FSelectionManager* GetSelectionManager() const { return SelectionManager; }
    UGizmoComponent* GetGizmo() const { return Gizmo; }

private:
    void HandleDragStart(const FRay& Ray);

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
    AActor* PilotedActor = nullptr;
    bool bIsPilotingActor = false;
    FVector SavedViewLocation = FVector(0.0f, 0.0f, 0.0f);
    FRotator SavedViewRotation;
    ELevelViewportType SavedViewportType = ELevelViewportType::Perspective;
    EEditorViewportPlayState PlayState = EEditorViewportPlayState::Stopped;
    float PaneToolbarHeight = 0.0f;
    FRect ViewportScreenRect;
    FRect ViewportFrameRect;

	FEditorViewportFrameInput CurrentInput;

    std::unique_ptr<FEditorViewportInputController> InputController;
};
