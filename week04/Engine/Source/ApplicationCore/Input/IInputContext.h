#pragma once

#pragma once
#include "InputTypes.h"
#include "InputState.h"

namespace Engine::ApplicationCore
{
    class IInputContext
    {
      public:
        virtual ~IInputContext() = default;

        virtual int  GetPriority() const = 0;
        virtual bool HandleEvent(const FInputEvent& Event, const FInputState& State) = 0;
        virtual void Tick(const FInputState& State) {}
    };
} // namespace Engine::ApplicationCore
