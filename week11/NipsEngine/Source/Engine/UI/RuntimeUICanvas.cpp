#include "UI/RuntimeUICanvas.h"

#include <algorithm>

void FRuntimeUICanvas::UpdateFromRenderContext(const FRuntimeUIRenderContext& Context)
{
    ComputedRect = FRuntimeUIRect(Context.ViewportMin, Context.ViewportSize);
    ComputedScale = 1.0f;

    if (ScaleMode == ERuntimeUICanvasScaleMode::ScaleWithScreenSize &&
        ReferenceResolution.X > 0.0f &&
        ReferenceResolution.Y > 0.0f)
    {
        const float ScaleX = Context.ViewportSize.X / ReferenceResolution.X;
        const float ScaleY = Context.ViewportSize.Y / ReferenceResolution.Y;
        ComputedScale = std::min(ScaleX, ScaleY);
    }
}
