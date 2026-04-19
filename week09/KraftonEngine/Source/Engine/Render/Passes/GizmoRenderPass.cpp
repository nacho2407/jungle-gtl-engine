#include "Render/Passes/GizmoRenderPass.h"
#include "Render/Core/RenderPassContext.h"
#include "Render/Core/FrameContext.h"
#include "Render/Types/RenderTypes.h"

void FGizmoRenderPass::Execute(FRenderPassContext& Context, const FFrameContext& Frame)
{
    Context.SubmitRenderPass(ERenderPass::GizmoOuter);
    Context.SubmitRenderPass(ERenderPass::GizmoInner);
}
