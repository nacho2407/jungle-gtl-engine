#include "EditorGlobalContext.h"

#include "EditorGlobalController.h"

bool FEditorGlobalContext::HandleEvent(const Engine::ApplicationCore::FInputEvent& Event,
                                       const Engine::ApplicationCore::FInputState& State)
{
    if (Controller == nullptr)
    {
        return false;
    }

    return Controller->HandleEvent(Event, State);
}

void FEditorGlobalContext::Tick(const Engine::ApplicationCore::FInputState& State) {}