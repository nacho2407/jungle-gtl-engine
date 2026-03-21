#include "ViewPortInputContext.h"

using Engine::ApplicationCore::EInputEventType;
using Engine::ApplicationCore::EKey;
using Engine::ApplicationCore::FInputEvent;
using Engine::ApplicationCore::FInputState;

bool FViewPortInputContext::HandleEvent(const FInputEvent& Event, const FInputState& State)
{
    if (Event.Type == EInputEventType::MouseButtonDown && Event.Key == EKey::MouseRight)
    {

    }
    if (Event.Type == EInputEventType::MouseButtonUp && Event.Key == EKey::MouseRight)
    {

    }
    
    return false;
}

void FViewPortInputContext::Tick(const Engine::ApplicationCore::FInputState& State)
{
    //  Camera 관련해서 State로 처리
}
