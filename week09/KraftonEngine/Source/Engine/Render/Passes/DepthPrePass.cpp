#include "Render/Passes/DepthPrePass.h"
#include "Render/Core/RenderPassContext.h"
#include "Render/Core/FrameContext.h"
#include "Render/Types/RenderTypes.h"

void FDepthPrePass::Execute(FRenderPassContext& Context, const FFrameContext& Frame)
{
    (void)Context; (void)Frame; // Reserved for a dedicated depth-prepass path.
}
