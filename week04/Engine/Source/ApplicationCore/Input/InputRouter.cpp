#include <Core/CoreMinimal.h>
#include "InputRouter.h"
#include <algorithm>

namespace Engine::ApplicationCore
{
    void FInputRouter::AddContext(IInputContext* Context)
    {
        Contexts.push_back(Context);

        std::sort(Contexts.begin(), Contexts.end(), [](IInputContext* A, IInputContext* B)
                  { return A->GetPriority() > B->GetPriority(); });
    }

    bool FInputRouter::RouteEvent(const FInputEvent& Event, const FInputState& State)
    {
        for (IInputContext* Context : Contexts)
        {
            if (Context && Context->HandleEvent(Event, State))
            {
                return true;
            }
        }
        return false;
    }

    void FInputRouter::TickContexts(const FInputState& State)
    {
        for (IInputContext* Context : Contexts)
        {
            if (Context)
            {
                Context->Tick(State);
            }
        }
    }
} // namespace Engine::ApplicationCore
