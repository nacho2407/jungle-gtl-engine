#pragma once

#include "Core/CoreMinimal.h"
#include "ApplicationCore/Input/InputContext.h"

namespace Engine::ApplicationCore
{
    class ENGINE_API FInputRouter
    {
    public:
        constexpr FInputRouter() = default;
        ~FInputRouter() = default;

        void AddContext(IInputContext* Context);
        bool RouteEvent(const FInputEvent& Event, const FInputState& State);
        void Tick(const FInputState& State);

      private:
        std::vector<IInputContext*> Contexts;
    };
} // namespace Engine::ApplicationCore