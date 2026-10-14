#include "UIPass.h"

#include "RenderPassRegistry.h"
#include "Render/Types/FrameContext.h"
#include "UI/UIManager.h"

REGISTER_RENDER_PASS(FUIPass)

FUIPass::FUIPass()
{
	PassType = ERenderPass::UI;
	RenderState = { EDepthStencilState::NoDepth, EBlendState::AlphaBlend,
	                ERasterizerState::SolidNoCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
}

bool FUIPass::BeginPass(const FPassContext& Ctx)
{
	return Ctx.Frame.ViewportRTV && UUIManager::Get().HasViewportWidgets();
}

void FUIPass::Execute(const FPassContext& Ctx)
{
	UUIManager::Get().Render(Ctx);
}
