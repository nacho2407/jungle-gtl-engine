#include "Render/Passes/SelectionMaskPass.h"
#include "Render/Core/RenderPassContext.h"
#include "Render/Core/FrameContext.h"
#include "Render/Types/RenderTypes.h"

void FSelectionMaskPass::Execute(FRenderPassContext& Context, const FFrameContext& Frame)
{
    Context.SubmitRenderPass(ERenderPass::SelectionMask);
}
