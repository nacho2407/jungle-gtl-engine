#include "ViewportInputContext.h"

using Engine::ApplicationCore::EInputEventType;
using Engine::ApplicationCore::EKey;
using Engine::ApplicationCore::FInputEvent;
using Engine::ApplicationCore::FInputState;

FViewportInputContext::FViewportInputContext(FViewportNavigationController* InNavigationController)
    : NavigationController(InNavigationController)
{
}

bool FViewportInputContext::HandleEvent(const FInputEvent& Event, const FInputState& State)
{
    if (NavigationController == nullptr)
    {
        return false;
    }

    switch (Event.Type)
    {
    case EInputEventType::MouseButtonDown:
        if (Event.Key == EKey::MouseRight)
        {
            bRightMouseDown = true;
            bFirstMouseMoveAfterRotateStart = true;
            NavigationController->SetRotating(true);
            return true;
        }
        break;
    case EInputEventType::MouseButtonUp:
        if (Event.Key == EKey::MouseRight)
        {
            bRightMouseDown = false;
            NavigationController->SetRotating(false);
            return true;
        }
        break;
    case EInputEventType::MouseMove:
        if (bRightMouseDown)
        {
            //  회전 시작 후 첫 번째 마우스 이동에 대해서 처리
            if (bFirstMouseMoveAfterRotateStart)
            {
                LastMouseX = Event.MouseX;
                LastMouseY = Event.MouseY;
                bFirstMouseMoveAfterRotateStart = false;
                return true;
            }

            int32 DeltaX = Event.MouseX - LastMouseX;
            int32 DeltaY = Event.MouseY - LastMouseY;

            //  마우스 이동량에 따라 카메라 회전 입력 추가
            NavigationController->AddYawInput(static_cast<float>(DeltaX));
            NavigationController->AddPitchInput(static_cast<float>(DeltaY));

            LastMouseX = Event.MouseX;
            LastMouseY = Event.MouseY;
            return true;
        }

        break;
    }

    return false;
}

void FViewportInputContext::Tick(const Engine::ApplicationCore::FInputState& State)
{
    if (NavigationController == nullptr)
    {
        return;
    }

    float ForwardValue = 0.f;
    float RightValue = 0.f;
    float UpValue = 0.f;

    if (State.IsKeyDown(EKey::W))
    {
        ForwardValue += 1.f;
    }
    if (State.IsKeyDown(EKey::S))
    {
        ForwardValue -= 1.f;
    }
    if (State.IsKeyDown(EKey::D))
    {
        RightValue += 1.f;
    }
    if (State.IsKeyDown(EKey::A))
    {
        RightValue -= 1.f;
    }
    if (State.IsKeyDown(EKey::E))
    {
        UpValue += 1.f;
    }
    if (State.IsKeyDown(EKey::Q))
    {
        UpValue -= 1.f;
    }

    //  입력된 값에 따라 이동 적용
    NavigationController->MoveForward(ForwardValue, DeltaTime);
    NavigationController->MoveRight(RightValue, DeltaTime);
    NavigationController->MoveUp(UpValue, DeltaTime);
}
