#include "Render/Passes/DecalPass.h"
#include "Render/Core/RenderPassContext.h"
#include "Render/Core/FrameContext.h"
#include "Render/Types/RenderTypes.h"

void FDecalPass::Execute(FRenderPassContext& Context, const FFrameContext& Frame)
{
    Context.SubmitRenderPass(ERenderPass::Decal);
}
