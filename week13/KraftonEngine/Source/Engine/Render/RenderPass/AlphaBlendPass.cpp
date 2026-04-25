#include "AlphaBlendPass.h"
#include "RenderPassRegistry.h"

REGISTER_RENDER_PASS(FAlphaBlendPass)

FAlphaBlendPass::FAlphaBlendPass()
{
	PassType    = ERenderPass::AlphaBlend;
	RenderState = { EDepthStencilState::Default, EBlendState::AlphaBlend,
	                ERasterizerState::SolidBackCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, true };
}
