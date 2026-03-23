#pragma once
#include "ApplicationCore/Input/InputContext.h"
#include "Viewport/Selection/ViewportSelectionController.h"
#include "Input/ContextModeTypes.h"

class FSelectionInputContext : public Engine::ApplicationCore::IInputContext
{
public:
    FSelectionInputContext(FViewportSelectionController* InSelectionController);
    ~FSelectionInputContext() override = default;

    //  현재는 Literal 저장
    int32 GetPriority() const override { return 20; }

    bool HandleEvent(const Engine::ApplicationCore::FInputEvent& Event,
                     const Engine::ApplicationCore::FInputState& State) override;
    void Tick(const Engine::ApplicationCore::FInputState& State) override;

private:
    ESelectionMode ResolveSelectionMode(const Engine::ApplicationCore::FInputState& State) const;
    
private:
    FViewportSelectionController* SelectionController = nullptr;
    
    bool bLeftMouseDown = false;
    bool bDragging = false;
    
    int32 PressMouseX = 0;
    int32 PressMouseY = 0;
    
    static constexpr int32 DragThreshold = 4;
};