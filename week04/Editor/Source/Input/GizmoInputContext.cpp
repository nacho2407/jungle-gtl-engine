#include "GizmoInputContext.h"

using Engine::ApplicationCore::EInputEventType;
using Engine::ApplicationCore::EKey;
using Engine::ApplicationCore::FInputEvent;
using Engine::ApplicationCore::FInputState;

FGizmoInputContext::FGizmoInputContext(FViewportGizmoController* InGizmoController)
    : GizmoController(InGizmoController)
{
}

bool FGizmoInputContext::HandleEvent(const Engine::ApplicationCore::FInputEvent& Event,
                                     const Engine::ApplicationCore::FInputState& State)
{
    if (GizmoController == nullptr)
    {
        return false;
    }

    switch (Event.Type)
    {
    case EInputEventType::MouseButtonDown:

        if (Event.Key == EKey::MouseLeft)
        {
            // 좌클릭: 기즈모 선택 및 드래그 시작
            GizmoController->OnMouseButtonDown(Event.MouseX, Event.MouseY);
            return true;
        }
        break;

    case EInputEventType::MouseButtonUp:
        if (Event.Key == EKey::MouseLeft)
        {
            // 좌클릭 해제: 기즈모 드래그 종료
            GizmoController->OnMouseButtonUp();
            return true;
        }
        break;

    case EInputEventType::MouseMove:
    {
        GizmoController->OnMouseMove(Event.MouseX, Event.MouseY);

       if (GizmoController->IsDragging())
        {
            return true;
        }
    }
    break;
    }

    return false;
}

void FGizmoInputContext::Tick(const Engine::ApplicationCore::FInputState& State)
{
    // GizmoController의 Tick이 필요하다면 여기서 호출해 줍니다.
    if (GizmoController)
    {
        GizmoController->Tick(DeltaTime);
    }
}