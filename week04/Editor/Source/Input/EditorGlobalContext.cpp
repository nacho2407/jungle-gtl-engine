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
    if (Event.Type != EInputEventType::KeyDown || Event.bRepeat)
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

    if (Controller == nullptr)
    {
        return false;
    }

    const bool bCtrlDown = State.Modifiers.bCtrlDown;
    const bool bShiftDown = State.Modifiers.bShiftDown;
    const bool bAltDown = State.Modifiers.bAltDown;

    if (bCtrlDown && !bAltDown)
    {
        if (Event.Key == EKey::S)
        {
            if (bShiftDown)
            {
                Controller->SaveSceneAs();
            }
            else
            {
                Controller->SaveScene();
            }

            return true;
        }

        if (!bShiftDown && Event.Key == EKey::O)
        {
            Controller->OpenScene();
            return true;
        }

        if (!bShiftDown && Event.Key == EKey::N)
        {
            Controller->NewScene();
            return true;
        }
    }

    if (!bCtrlDown && !bShiftDown && !bAltDown && Event.Key == EKey::F1)
    {
        Controller->RequestAboutPopup();
        return true;
    }

    if (!bCtrlDown && !bShiftDown && !bAltDown && Event.Key == EKey::F)
    {
        if (NavigationController == nullptr)
        {
            return false;
        }

        NavigationController->FocusActors();
        return true;
    }

    if (bCtrlDown || bShiftDown || bAltDown || Event.Key != EKey::Delete)
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