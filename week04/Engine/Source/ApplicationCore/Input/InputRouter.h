#pragma once

#pragma once
#include <vector>
#include "InputContext.h"

namespace Engine::ApplicationCore
{
    class ENGINE_API FInputRouter
    {
      public:
        constexpr FInputRouter() = default;
        ~FInputRouter() = default;

        void AddContext(IInputContext* Context);
        bool RouteEvent(const FInputEvent& Event, const FInputState& State);
        void TickContexts(const FInputState& State);

      private:
        std::vector<IInputContext*> Contexts;
    };
} // namespace Engine::ApplicationCore