#include "Input/ViewportOverlayInputContext.h"
#include "Viewport/Window/WindowOverlayManager.h"
#include "Viewport/Window/EditorViewportPanel.h"
#include "Viewport/EditorViewportClient.h"

using Engine::ApplicationCore::EInputEventType;
using Engine::ApplicationCore::EKey;
using Engine::ApplicationCore::FInputEvent;
using Engine::ApplicationCore::FInputState;

FViewportOverlayInputContext::FViewportOverlayInputContext(FWindowOverlayManager* InOverlayManager)
    : OverlayManager(InOverlayManager)
{
}

bool FViewportOverlayInputContext::HandleEvent(const FInputEvent& Event, const FInputState& State)
{
    if (!OverlayManager)
    {
        return false;
    }

    switch (Event.Type)
    {
    // ── Mouse button down ────────────────────────────────────────────────────
    case EInputEventType::MouseButtonDown:
    {
        // Only left-click can start a splitter drag.
        if (Event.Key == EKey::MouseLeft)
        {
            const bool bHitV = OverlayManager->HitTestSplitterV(Event.MouseX, Event.MouseY);
            const bool bHitH = OverlayManager->HitTestSplitterH(Event.MouseX, Event.MouseY);
            if (bHitV || bHitH)
            {
                OverlayManager->BeginSplitterDrag(bHitV, bHitH);
                LastMouseX = Event.MouseX;
                LastMouseY = Event.MouseY;
                return true;
            }
        }

        // No splitter hit — lock input to the panel under the cursor.
        if (CapturePanel == nullptr)
        {
            CapturePanel = OverlayManager->FindPanelAtPoint(Event.MouseX, Event.MouseY);
        }
        if (CapturePanel && CapturePanel->ViewportClient)
        {
            CapturePanel->ViewportClient->HandleInputEvent(Event, State);
        }
        return true;
    }

    // ── Mouse move ───────────────────────────────────────────────────────────
    case EInputEventType::MouseMove:
    {
        if (OverlayManager->IsDraggingSplitter())
        {
            const float DeltaX = static_cast<float>(Event.MouseX - LastMouseX);
            const float DeltaY = static_cast<float>(Event.MouseY - LastMouseY);
            OverlayManager->UpdateSplitterDrag(DeltaX, DeltaY);
            LastMouseX = Event.MouseX;
            LastMouseY = Event.MouseY;
            return true;
        }

        // Forward to captured panel, or whichever panel the cursor is over.
        FEditorViewportPanel* Target = CapturePanel
            ? CapturePanel
            : OverlayManager->FindPanelAtPoint(Event.MouseX, Event.MouseY);

        if (Target && Target->ViewportClient)
        {
            Target->ViewportClient->HandleInputEvent(Event, State);
        }
        return true;
    }

    // ── Mouse button up ──────────────────────────────────────────────────────
    case EInputEventType::MouseButtonUp:
    {
        if (OverlayManager->IsDraggingSplitter())
        {
            OverlayManager->EndSplitterDrag();
            return true;
        }

        if (CapturePanel && CapturePanel->ViewportClient)
        {
            CapturePanel->ViewportClient->HandleInputEvent(Event, State);
        }

        // Release capture once no mouse button remains held.
        const bool bAnyButtonHeld = State.IsKeyDown(EKey::MouseLeft)  ||
                                    State.IsKeyDown(EKey::MouseRight)  ||
                                    State.IsKeyDown(EKey::MouseMiddle);
        if (!bAnyButtonHeld)
        {
            CapturePanel = nullptr;
        }
        return true;
    }

    // ── Mouse wheel ──────────────────────────────────────────────────────────
    case EInputEventType::MouseWheel:
    {
        FEditorViewportPanel* Target = CapturePanel
            ? CapturePanel
            : OverlayManager->FindPanelAtPoint(Event.MouseX, Event.MouseY);

        if (Target && Target->ViewportClient)
        {
            Target->ViewportClient->HandleInputEvent(Event, State);
        }
        return true;
    }

    // ── Keyboard ─────────────────────────────────────────────────────────────
    // Forward to the captured panel but do NOT consume — global shortcuts
    // (Delete, Save, etc.) in GlobalInputContext must also see keyboard events.
    case EInputEventType::KeyDown:
    case EInputEventType::KeyUp:
    {
        if (CapturePanel && CapturePanel->ViewportClient)
        {
            CapturePanel->ViewportClient->HandleInputEvent(Event, State);
        }
        return false;
    }

    default:
        break;
    }

    return false;
}
