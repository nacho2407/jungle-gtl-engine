#include "EditorGlobalContext.h"
#include "Input/EditorGlobalContext.h"

#include "Viewport/Global/EditorGlobalController.h"
#include "Viewport/Navigation/ViewportNavigationController.h"

#include "imgui.h"

using Engine::ApplicationCore::EInputEventType;
using Engine::ApplicationCore::EKey;

bool FEditorGlobalContext::HandleEvent(const Engine::ApplicationCore::FInputEvent& Event,
                                       const Engine::ApplicationCore::FInputState& State)
{
    (void)State;

    if (Event.Type != EInputEventType::KeyDown || Event.bRepeat)
    {
        return false;
    }

    if (Event.Modifiers.bAltDown || Event.Modifiers.bCtrlDown || Event.Modifiers.bShiftDown)
    {
        return false;
    }

    if (ImGui::GetCurrentContext() != nullptr)
    {
        const ImGuiIO& IO = ImGui::GetIO();
        if (IO.WantCaptureKeyboard || IO.WantTextInput)
        {
            return false;
        }
    }

    if (Event.Key == EKey::F)
    {
        if (NavigationController == nullptr)
        {
            return false;
        }

        NavigationController->FocusActors();
        return true;
    }

    if (Event.Key != EKey::Delete)
    {
        return false;
    }

    if (Controller == nullptr)
    {
        return false;
    }

    if (!Controller->CanDeleteSelectedActors())
    {
        return false;
    }

    return Controller->DeleteSelectedActors();
}

void FEditorGlobalContext::Tick(const Engine::ApplicationCore::FInputState& State) {}