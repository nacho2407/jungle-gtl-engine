#include "EditorGlobalController.h"

#include "imgui.h"

using Engine::ApplicationCore::EInputEventType;
using Engine::ApplicationCore::EKey;
using Engine::ApplicationCore::FInputEvent;
using Engine::ApplicationCore::FInputState;

void FEditorGlobalController::BindDeleteSelection(FCanExecuteHandler InCanExecute,
                                                  FExecuteHandler InExecute)
{
    CanDeleteSelection = std::move(InCanExecute);
    ExecuteDeleteSelection = std::move(InExecute);
}

bool FEditorGlobalController::HandleEvent(const FInputEvent& Event, const FInputState& State) const
{
    (void)State;

    if (Event.Type != EInputEventType::KeyDown || Event.bRepeat)
    {
        return false;
    }

    if (Event.Key != EKey::Delete)
    {
        return false;
    }

    if (!CanHandleKeyboardEvent())
    {
        return false;
    }

    if (!CanDeleteSelection || !ExecuteDeleteSelection)
    {
        return false;
    }

    if (!CanDeleteSelection())
    {
        return false;
    }

    ExecuteDeleteSelection();
    return true;
}

bool FEditorGlobalController::CanHandleKeyboardEvent() const
{
    if (ImGui::GetCurrentContext() == nullptr)
    {
        return true;
    }

    const ImGuiIO& IO = ImGui::GetIO();
    return !IO.WantCaptureKeyboard && !IO.WantTextInput;
}
