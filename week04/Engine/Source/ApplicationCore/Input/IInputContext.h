#pragma once

#include "ApplicationCore/Input/InputTypes.h"
#include "ApplicationCore/Input/InputState.h"

//  TODO : 전방 선언 지우기
struct FInputEvent;

namespace Engine::ApplicationCore
{
    class ENGINE_API IInputContext
    {
    public:
        virtual ~IInputContext() = default;
        
        virtual int32 GetPriority() const = 0;
        virtual bool HandleEvent(const FInputEvent & Event, const FInputState & State) = 0;
        
        virtual void Tick(const FInputState & State) = 0;
    };
}