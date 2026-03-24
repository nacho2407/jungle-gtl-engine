#include "NavigationInputContext.h"

#if defined(_WIN32)
#include <Windows.h>
#endif

using Engine::ApplicationCore::EInputEventType;
using Engine::ApplicationCore::EKey;
using Engine::ApplicationCore::FInputEvent;
using Engine::ApplicationCore::FInputState;

FNavigationInputContext::FNavigationInputContext(FViewportNavigationController* InNavigationController)
    : NavigationController(InNavigationController)
{
}

bool FNavigationInputContext::HandleEvent(const FInputEvent& Event, const FInputState& State)
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
#if defined(_WIN32)
            while (::ShowCursor(FALSE) >= 0)
            {
            }
#endif
            return true;
        }

        if (Event.Key == EKey::MouseLeft && State.Modifiers.bAltDown)
        {
            bOrbitLeftMouseDown = true;
            bFirstMouseMoveAfterOrbitStart = true;
            NavigationController->BeginOrbit();
#if defined(_WIN32)
            while (::ShowCursor(FALSE) >= 0)
            {
            }
#endif
            return true;
        }

        if (Event.Key == EKey::MouseLeft)
        {
            bLeftMouseDown = true;
            bPendingLeftMouseRotate = true;
            bLeftRotationActive = false;
            LeftMouseDownStartX = Event.MouseX;
            LeftMouseDownStartY = Event.MouseY;
            LastMouseX = Event.MouseX;
            LastMouseY = Event.MouseY;
            return false;
        }
        break;

    case EInputEventType::MouseButtonUp:
        if (Event.Key == EKey::MouseLeft && bOrbitLeftMouseDown)
        {
            bOrbitLeftMouseDown = false;
            NavigationController->EndOrbit();
#if defined(_WIN32)
            while (::ShowCursor(TRUE) < 0)
            {
            }
#endif
            return true;
        }

        if (Event.Key == EKey::MouseRight)
        {
            bRightMouseDown = false;
            NavigationController->SetRotating(false);
#if defined(_WIN32)
            while (::ShowCursor(TRUE) < 0)
            {
            }
#endif
            return true;
        }

        if (Event.Key == EKey::MouseLeft)
        {
            bLeftMouseDown = false;
            bPendingLeftMouseRotate = false;

            if (bLeftRotationActive)
            {
                bLeftRotationActive = false;
                NavigationController->SetRotating(false);
#if defined(_WIN32)
                while (::ShowCursor(TRUE) < 0)
                {
                }
#endif
                return true;
            }

            return false;
        }
        break;

    case EInputEventType::MouseMove:
        if (bOrbitLeftMouseDown)
        {
            return HandleMouseMove(Event, bFirstMouseMoveAfterOrbitStart);
        }

        if (bRightMouseDown)
        {
            return HandleMouseMove(Event, bFirstMouseMoveAfterRotateStart);
        }

        if (bLeftMouseDown)
        {
            if (bPendingLeftMouseRotate)
            {
                const int32 DragDeltaX = Event.MouseX - LeftMouseDownStartX;
                const int32 DragDeltaY = Event.MouseY - LeftMouseDownStartY;
                const float DragDistanceSq = static_cast<float>(DragDeltaX * DragDeltaX + DragDeltaY * DragDeltaY);
                const float ThresholdSq = LeftMouseDragThreshold * LeftMouseDragThreshold;

                if (DragDistanceSq >= ThresholdSq)
                {
                    bPendingLeftMouseRotate = false;
                    bLeftRotationActive = true;
                    bFirstMouseMoveAfterRotateStart = true;
                    NavigationController->SetRotating(true);
#if defined(_WIN32)
                    while (::ShowCursor(FALSE) >= 0)
                    {
                    }
#endif
                    return HandleMouseMove(Event, bFirstMouseMoveAfterRotateStart);
                }

                return false;
            }

            if (bLeftRotationActive)
            {
                return HandleMouseMove(Event, bFirstMouseMoveAfterRotateStart);
            }
        }

        break;

    case EInputEventType::MouseWheel:
        if (bRightMouseDown || bLeftRotationActive)
        {
            const float SpeedStep = (Event.WheelDelta > 0) ? 20.0f : -20.0f;
            NavigationController->AdjustMoveSpeed(SpeedStep);
        }
        else
        {
            NavigationController->ModifyFOV(static_cast<float>(-Event.WheelDelta));
        }
        return true;

        // break;
    default:
        break;
    }
    
    return false;
}

void FNavigationInputContext::Tick(const Engine::ApplicationCore::FInputState& State)
{
    if (NavigationController == nullptr)
    {
        return;
    }

    //  중간에 Alt 키가 떼어지면 Orbiting 종료
    const bool bAltDown = State.Modifiers.bAltDown;
    if (bOrbitLeftMouseDown && !bAltDown)
    {
        bOrbitLeftMouseDown = false;
        NavigationController->EndOrbit();
#if defined(_WIN32)
        while (::ShowCursor(TRUE) < 0)
        {
        }
#endif
    }

    //  Mouse Button이 눌리지 않았다면, 이동 금지
    if (!bRightMouseDown && !bLeftRotationActive)
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

    FVector NormalizedInput(ForwardValue, RightValue, UpValue);

    NormalizedInput = NormalizedInput.GetSafeNormal();

    //  입력된 값에 따라 이동 적용
    NavigationController->MoveForward(NormalizedInput.X, DeltaTime);
    NavigationController->MoveRight(NormalizedInput.Y, DeltaTime);
    NavigationController->MoveUp(NormalizedInput.Z, DeltaTime);
}

bool FNavigationInputContext::HandleMouseMove(const FInputEvent& Event, bool& bFirstMove)
{
    if (bFirstMove)
    {
        LastMouseX = Event.MouseX;
        LastMouseY = Event.MouseY;
        bFirstMove = false;
        return true;
    }

    const int32 DeltaX = Event.MouseX - LastMouseX;
    const int32 DeltaY = Event.MouseY - LastMouseY;

    NavigationController->AddYawInput(static_cast<float>(DeltaX));
    NavigationController->AddPitchInput(static_cast<float>(-DeltaY));

    LastMouseX = Event.MouseX;
    LastMouseY = Event.MouseY;

    return true;
}