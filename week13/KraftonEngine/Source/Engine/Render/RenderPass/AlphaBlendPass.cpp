#include "AlphaBlendPass.h"
#include "RenderPassRegistry.h"

REGISTER_RENDER_PASS(FAlphaBlendPass)

FAlphaBlendPass::FAlphaBlendPass()
{
	PassType    = ERenderPass::AlphaBlend;
	RenderState = { EDepthStencilState::DepthReadOnly, EBlendState::AlphaBlend,
	                ERasterizerState::SolidNoCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, true };
}
