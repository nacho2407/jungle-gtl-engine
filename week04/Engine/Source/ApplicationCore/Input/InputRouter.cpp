#include "Core/CoreMinimal.h"
#include "InputRouter.h"
#include <algorithm>

namespace Engine::ApplicationCore
{
    void FInputRouter::AddContext(IInputContext* Context)
    {
        //  Emplace back 교체 가능하긴 함
        Contexts.push_back(Context);
        
        //  TODO : sort 함수 Wrapping 하기
        //  Priority number가 낮을 수록 우선순위 높음
        std::sort(Contexts.begin(), Contexts.end(), [](IInputContext* A, IInputContext* B)
                  { return A->GetPriority() > B->GetPriority(); });
    }

    //  Event가 Context에 포함되면 true, 정의된 Context가 아니라면 false 반환
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
