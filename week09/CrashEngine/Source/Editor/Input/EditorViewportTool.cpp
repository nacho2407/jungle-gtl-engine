#include "EditorViewportTool.h"

#include "Component/CameraComponent.h"
#include "Editor/Viewport/EditorViewportClient.h"
#include "Viewport/Viewport.h"

bool FEditorViewportTool::BuildMouseRay(FRay& OutRay) const
{
    if (!Owner || !Owner->GetCamera())
    {
        return false;
    }

    FViewport* Viewport = Owner->GetViewport();
    const FRect& ViewportRect = Owner->GetViewportScreenRect();

    const float VPWidth = Viewport ? static_cast<float>(Viewport->GetWidth()) : ViewportRect.Width;
    const float VPHeight = Viewport ? static_cast<float>(Viewport->GetHeight()) : ViewportRect.Height;
    if (VPWidth <= 0.0f || VPHeight <= 0.0f)
    {
        return false;
    }

    const FEditorViewportFrameInput& Input = Owner->GetCurrentInput();
    const float LocalMouseX = static_cast<float>(Input.MouseLocalPos.x);
    const float LocalMouseY = static_cast<float>(Input.MouseLocalPos.y);

    OutRay = Owner->GetCamera()->DeprojectScreenToWorld(LocalMouseX, LocalMouseY, VPWidth, VPHeight);
    return true;
}
