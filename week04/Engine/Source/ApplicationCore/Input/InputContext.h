#pragma once

#pragma once
#include "InputTypes.h"
#include "InputState.h"

namespace Engine::ApplicationCore
{
    class ENGINE_API IInputContext
    {
      public:
        IInputContext() = default;
        virtual ~IInputContext() = default;

        virtual int  GetPriority() const = 0;
        virtual bool HandleEvent(const FInputEvent& Event, const FInputState& State) = 0;
        virtual void Tick(const FInputState& State) {}

        private:
        int Priority{0};
    };
} // namespace Engine::ApplicationCore
