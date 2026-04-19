#include "Render/Passes/AdditiveDecalPass.h"
#include "Render/Core/RenderPassContext.h"
#include "Render/Core/FrameContext.h"
#include "Render/Types/RenderTypes.h"

void FAdditiveDecalPass::Execute(FRenderPassContext& Context, const FFrameContext& Frame)
{
    Context.SubmitRenderPass(ERenderPass::AdditiveDecal);
}
