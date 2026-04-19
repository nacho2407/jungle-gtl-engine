#include "Render/Passes/AlphaBlendPass.h"
#include "Render/Core/RenderPassContext.h"
#include "Render/Core/FrameContext.h"
#include "Render/Types/RenderTypes.h"

void FAlphaBlendPass::Execute(FRenderPassContext& Context, const FFrameContext& Frame)
{
    Context.SubmitRenderPass(ERenderPass::AlphaBlend);
}
