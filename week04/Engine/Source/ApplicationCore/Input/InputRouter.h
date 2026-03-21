#pragma once

#pragma once
#include <vector>
#include "IInputContext.h"

namespace Engine::ApplicationCore
{
    class ENGINE_API FInputRouter
    {
      public:
        void AddContext(IInputContext* Context);
        bool RouteEvent(const FInputEvent& Event, const FInputState& State);
        void TickContexts(const FInputState& State);

      private:
        std::vector<IInputContext*> Contexts;
    };
} // namespace Engine::ApplicationCore