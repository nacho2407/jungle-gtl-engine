#include "Render/Core/RenderPassContext.h"
#include "Render/Renderer/Renderer.h"

void FRenderPassContext::SubmitRenderPass(ERenderPass Pass)
{
    Renderer.SubmitRenderPass(Pass);
}

void FRenderPassContext::SubmitRenderPassByUserBits(ERenderPass Pass, uint16 UserBits)
{
    Renderer.SubmitRenderPassByUserBits(Pass, UserBits);
}
