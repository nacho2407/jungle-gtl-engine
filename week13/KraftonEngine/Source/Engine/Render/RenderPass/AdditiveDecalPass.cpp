#include "AdditiveDecalPass.h"
#include "RenderPassRegistry.h"

REGISTER_RENDER_PASS(FAdditiveDecalPass)

FAdditiveDecalPass::FAdditiveDecalPass()
{
	PassType    = ERenderPass::AdditiveDecal;
	RenderState = { EDepthStencilState::DepthReadOnly, EBlendState::Additive,
	                ERasterizerState::SolidNoCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, true };
}
