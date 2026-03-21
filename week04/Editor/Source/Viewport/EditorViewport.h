#pragma once

#include <ApplicationCore/Input/InputState.h>
#include <ApplicationCore/Input/InputRouter.h>

class FEditorViewport
{
  public:
    void Create();
    void Release();
    void Initialize();

    void RouteInput(const Engine::ApplicationCore::FInputEvent& Event,
                    const Engine::ApplicationCore::FInputState& State);

    void TickInput(const Engine::ApplicationCore::FInputState& State);
    void Tick();
	
private:
    Engine::ApplicationCore::FInputRouter InputRouter;
    //FViewport
};