#pragma once

#include "Core/CoreMinimal.h"
#include "ApplicationCore/Input/InputContext.h"

class FWindowOverlayManager;
class FEditorViewportPanel;

class FViewportOverlayInputContext : public Engine::ApplicationCore::IInputContext
{
public:
    explicit FViewportOverlayInputContext(FWindowOverlayManager* InOverlayManager);
    ~FViewportOverlayInputContext() override = default;

    // Runs before GlobalInputContext (priority 10) so splitter drag
    // and panel routing preempt global keyboard shortcuts for mouse events.
    int  GetPriority() const override { return 5; }
    bool HandleEvent(const Engine::ApplicationCore::FInputEvent& Event,
                     const Engine::ApplicationCore::FInputState& State) override;

    FEditorViewportPanel* GetCapturePanel() const { return CapturePanel; }

private:
    FWindowOverlayManager* OverlayManager = nullptr;
    FEditorViewportPanel*  CapturePanel   = nullptr;

    int32 LastMouseX = 0;
    int32 LastMouseY = 0;
};
