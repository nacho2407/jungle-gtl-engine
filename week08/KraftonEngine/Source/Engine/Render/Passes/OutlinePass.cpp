#include "Render/Passes/OutlinePass.h"
#include "Render/Core/RenderPassContext.h"
#include "Render/Core/FrameContext.h"
#include "Render/Types/RenderTypes.h"

void FOutlinePass::Execute(FRenderPassContext& Context, const FFrameContext& Frame)
{
    Context.SubmitRenderPassByUserBits(ERenderPass::PostProcess, 1);
}
