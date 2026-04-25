#include "DecalPass.h"
#include "RenderPassRegistry.h"

REGISTER_RENDER_PASS(FDecalPass)

FDecalPass::FDecalPass()
{
	PassType    = ERenderPass::Decal;
	RenderState = { EDepthStencilState::DepthReadOnly, EBlendState::AlphaBlend,
	                ERasterizerState::SolidNoCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, true };
}
