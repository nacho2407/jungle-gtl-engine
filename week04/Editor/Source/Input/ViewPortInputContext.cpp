#include "ViewPortInputContext.h"

bool FViewPortInputContext::HandleEvent(const Engine::ApplicationCore::FInputEvent& Event,
    const Engine::ApplicationCore::FInputState&                                     State)
{
    //  TODO : Event Type 관련해서 Event 분기 처리 하기
    return false;
}

void FViewPortInputContext::Tick(const Engine::ApplicationCore::FInputState& State)
{
    //  Camera 관련해서 State로 처리
}

