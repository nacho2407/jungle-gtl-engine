#pragma once

#include "Core/CoreMinimal.h"

#include "ApplicationCore/Input/InputContext.h"
#include "Viewport/Navigation/ViewportNavigationController.h"

namespace
{
    //enum class
}

class FNavigationInputContext : public Engine::ApplicationCore::IInputContext
{
public:
    FNavigationInputContext(FViewportNavigationController* InNavigationController);
    ~FNavigationInputContext() override = default;

    //  현재는 Literal 저장
    int32 GetPriority() const override { return 50; }
    bool  HandleEvent(const Engine::ApplicationCore::FInputEvent& Event,
                     const Engine::ApplicationCore::FInputState&  State) override;
    void Tick(const Engine::ApplicationCore::FInputState& State) override;

    void SetDeltaTime(float InDeltaTime) { DeltaTime = InDeltaTime; }

private:
    bool HandleMouseMove(const Engine::ApplicationCore::FInputEvent& Event, bool& bFirstMove);

private:
    FViewportNavigationController* NavigationController = nullptr;

    bool bRightMouseDown = false;
    bool bLeftMouseDown = false;
    
    bool bOrbitLeftMouseDown = false;
    bool bDollyRightMouseDown = false;
    
    bool bLeftRotationActive = false;
    bool bPendingLeftMouseRotate = false;
    
    bool bFirstMouseMoveAfterRotateStart = false;
    bool bFirstMouseMoveAfterOrbitStart = false;
    bool bFirstMouseMoveAfterDollyStart = false;

    int32 LastMouseX = 0;
    int32 LastMouseY = 0;
    int32 LeftMouseDownStartX = 0;
    int32 LeftMouseDownStartY = 0;

    float DeltaTime = 0.016f; // 60 FPS 기준 초기값
    float LeftMouseDragThreshold = 4.0f;
    
    float DollyDragSensitivity = 0.1f;
};