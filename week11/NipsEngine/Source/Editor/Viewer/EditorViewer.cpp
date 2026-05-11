#include "EditorViewer.h"
#include "EditorEngine.h"
#include "Editor/EditorRenderPipeline.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Geometry/Ray.h"
#include "Component/GizmoComponent.h"
#include "imgui.h"

void FEditorViewer::Init(
    FWindowsWindow* InWindow,
    UEditorEngine* InEditor,
    UWorld* InWorld,
    FSelectionManager* InSelectionManager)
{
    Window = InWindow;
    Editor = InEditor;

    Viewport.SetClient(&Client);

    Client.Initialize(InWindow, InEditor);
    Client.SetWorld(InWorld);
    Client.SetGizmo(InSelectionManager->GetGizmo());
    Client.SetSelectionManager(InSelectionManager);

    Client.SetViewport(&Viewport);
    Client.SetState(&Viewport.GetState());

    Client.SetViewportType(EEditorViewportType::EVT_Perspective);
    Client.CreateCamera();
    Client.ApplyCameraMode();

	// 임시
	FViewportRect Rect = { 0, 0, 300, 300 };
    Viewport.SetRect(Rect);
}

void FEditorViewer::Shutdown()
{
    Viewport.SetClient(nullptr);
}

void FEditorViewer::Tick(float DeltaTime)
{
    // Update hover state for this standalone viewer viewport so gizmo logic
    // that depends on bHovered runs correctly even though the viewer is
    // outside the main viewport layout's loop.
    InputSystem& IS = InputSystem::Get();
    POINT MousePoint = IS.GetMousePos();
    if (Window) MousePoint = Window->ScreenToClientPoint(MousePoint);

    const FViewportRect& Rect = Viewport.GetRect();
    const bool bHovered = (Rect.Width > 0 && Rect.Height > 0)
        && (MousePoint.x >= Rect.X && MousePoint.x < Rect.X + Rect.Width)
        && (MousePoint.y >= Rect.Y && MousePoint.y < Rect.Y + Rect.Height);

    if (FEditorViewportClient* ViewerClient = static_cast<FEditorViewportClient*>(Viewport.GetClient()))
    {
        if (FEditorViewportState* State = ViewerClient->GetViewportState())
        {
            State->bHovered = bHovered;
        }

        UGizmoComponent* Gizmo = ViewerClient->GetGizmo();
        FViewportCamera* Cam = ViewerClient->GetCamera();

        // Hover raycast to update hovered axis
        if (bHovered && Gizmo && Cam && Rect.Width > 0 && Rect.Height > 0)
        {
            const float LocalX = static_cast<float>(MousePoint.x - Rect.X);
            const float LocalY = static_cast<float>(MousePoint.y - Rect.Y);
            const FRay MouseRay = Cam->DeprojectScreenToWorld(LocalX, LocalY, static_cast<float>(Rect.Width), static_cast<float>(Rect.Height));
            FHitResult Hit{};
            Gizmo->RaycastMesh(MouseRay, Hit);
        }

        // Dragging logic for gizmo: emulate editor viewport behavior
        // Detect initial press
        if (IS.GetKeyDown(VK_LBUTTON))
        {
            LeftMouseDownPos = MousePoint;
            if (bHovered && Gizmo && Cam && Rect.Width > 0 && Rect.Height > 0)
            {
                const float LocalX = static_cast<float>(MousePoint.x - Rect.X);
                const float LocalY = static_cast<float>(MousePoint.y - Rect.Y);
                const FRay MouseRay = Cam->DeprojectScreenToWorld(LocalX, LocalY, static_cast<float>(Rect.Width), static_cast<float>(Rect.Height));
                FHitResult Hit{};
                if (Gizmo->RaycastMesh(MouseRay, Hit))
                {
                    bLeftPressedOnHandle = true;
                    Gizmo->SetPressedOnHandle(true);
                }
                else
                {
                    bLeftPressedOnHandle = false;
                }
            }
            else
            {
                bLeftPressedOnHandle = false;
            }
        }

        // Start holding when drag threshold exceeded
        if (!bLeftHolding && IS.GetLeftDragging() && bLeftPressedOnHandle)
        {
            bLeftHolding = true;
            if (Gizmo)
                Gizmo->SetHolding(true);
        }

        // While holding, update drag
        if (bLeftHolding && Gizmo && Cam && Rect.Width > 0 && Rect.Height > 0)
        {
            const float LocalX = static_cast<float>(MousePoint.x - Rect.X);
            const float LocalY = static_cast<float>(MousePoint.y - Rect.Y);
            const FRay MouseRay = Cam->DeprojectScreenToWorld(LocalX, LocalY, static_cast<float>(Rect.Width), static_cast<float>(Rect.Height));
            Gizmo->UpdateDrag(MouseRay);
        }

        // Drag end
        if (IS.GetLeftDragEnd())
        {
            if (bLeftHolding && Gizmo)
            {
                Gizmo->DragEnd();
            }
            bLeftHolding = false;
            bLeftPressedOnHandle = false;
        }

        // Mouse button up without drag
        if (IS.GetKeyUp(VK_LBUTTON))
        {
            if (!bLeftHolding && bLeftPressedOnHandle && Gizmo)
            {
                // click release without drag — disarm pressed state
                Gizmo->SetPressedOnHandle(false);
            }
            bLeftPressedOnHandle = false;
            bLeftHolding = false;
        }
    }

    Client.Tick(DeltaTime);
}

void FEditorViewer::Resize(int32 Width, int32 Height)
{
    if (Width <= 0 || Height <= 0)
    {
        return;
    }

    const FViewportRect& CurRect = Viewport.GetRect();

    if (CurRect.Width == Width && CurRect.Height == Height)
    {
        return; // 변화 없음
    }

    FViewportRect NewRect = {
        CurRect.X,
        CurRect.Y,
        Width,
        Height
    };

    Viewport.SetRect(NewRect);
    Client.SetViewportSize((float)Width, (float)Height);
}
