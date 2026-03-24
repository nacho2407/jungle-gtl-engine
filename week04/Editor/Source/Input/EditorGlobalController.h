#pragma once

#include <functional>

#include "ApplicationCore/Input/InputState.h"
#include "ApplicationCore/Input/InputTypes.h"

class FEditorGlobalController
{
public:
    using FCanExecuteHandler = std::function<bool()>;
    using FExecuteHandler = std::function<void()>;

    void BindDeleteSelection(FCanExecuteHandler InCanExecute, FExecuteHandler InExecute);

    bool HandleEvent(const Engine::ApplicationCore::FInputEvent& Event,
                     const Engine::ApplicationCore::FInputState& State) const;

private:
    bool CanHandleKeyboardEvent() const;

private:
    FCanExecuteHandler CanDeleteSelection;
    FExecuteHandler    ExecuteDeleteSelection;
};
