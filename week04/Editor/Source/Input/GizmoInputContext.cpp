#include "GizmoInputContext.h"
#include "Viewport/Navigation/ViewportNavigationController.h"

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
    if (GizmoController == nullptr || !GizmoController->bIsDrawed)
    {
        return false;
    }

    switch (Event.Type)
    {
        {
        case EInputEventType::MouseButtonDown:

            if (Event.Key == EKey::MouseLeft)
            {

                const bool bStartedDrag =
                    GizmoController->OnMouseButtonDown(Event.MouseX, Event.MouseY);

                //  Shift Drag인지 판단
                bFollowViewportCamera = bStartedDrag && State.Modifiers.bShiftDown &&
                                        NavigationController != nullptr &&
                                        GizmoController->GetGizmoType() == EGizmoType::Translation;

                GizmoController->SetTranslationDragScale(
                    bFollowViewportCamera ? ShiftDragTranslationScale : 1.0f);

                if (bFollowViewportCamera)
                {
                    NavigationController->SetGizmoFollowSpeedScale(1.0f);
                }
#if defined(_WIN32)
                while (::ShowCursor(FALSE) >= 0)
                {
                }
#endif
                return bStartedDrag;
            }
            break;
        }

    case EInputEventType::MouseButtonUp:
    {
        if (Event.Key == EKey::MouseLeft)
        {
            bFollowViewportCamera = false;
            GizmoController->SetTranslationDragScale(1.0f);

            bool bWasDragging = GizmoController->IsDragging();
            GizmoController->OnMouseButtonUp();
#if defined(_WIN32)
            while (::ShowCursor(TRUE) < 0)
            {
            }
#endif
            return bWasDragging;
        }
        break;
    }

    case EInputEventType::MouseMove:
    {

        GizmoController->OnMouseMove(Event.MouseX, Event.MouseY);

        bFollowViewportCamera = GizmoController->IsDragging() && State.IsKeyDown(EKey::MouseLeft) &&
                                State.Modifiers.bShiftDown && NavigationController != nullptr &&
                                GizmoController->GetGizmoType() == EGizmoType::Translation;

        GizmoController->SetTranslationDragScale(bFollowViewportCamera ? ShiftDragTranslationScale
                                                                       : 1.0f);

        if (bFollowViewportCamera)
        {
            FVector Delta = GizmoController->ConsumeDelta();
            NavigationController->TranslateWithGizmoDelta(Delta);
        }

        if (GizmoController->IsDragging())
        {
#if defined(_WIN32)
            while (::ShowCursor(FALSE) >= 0)
            {
            }
#endif
            return true;
        }
        break;
    }
    case EInputEventType::KeyDown:
    {
        if (Event.Key == EKey::Space)
        {
            GizmoController->ChangeGizmoType();
            return true;
        }
        else if (Event.Key == EKey::X)
        {
            GizmoController->ChangeWorldMode();
            return true;
        }
        else if (Event.Key == EKey::N1)
        {
            bool  bSnapping;
            float Dummy;
            GizmoController->GetTranslateSnapping(bSnapping, Dummy);
            GizmoController->SetTranslateSnapping(!bSnapping, Dummy);
            return true;
        }
        else if (Event.Key == EKey::N2)
        {
            bool  bSnapping;
            float Dummy;
            GizmoController->GetRotateSnapping(bSnapping, Dummy);
            GizmoController->SetRotateSnapping(!bSnapping, Dummy);
            return true;
        }
        else if (Event.Key == EKey::N3)
        {
            bool  bSnapping;
            float Dummy;
            GizmoController->GetScaleSnapping(bSnapping, Dummy);
            GizmoController->SetScaleSnapping(!bSnapping, Dummy);
            return true;
        }

        return false; // 처리하지 않은 키는 다른 컨텍스트로 전파
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