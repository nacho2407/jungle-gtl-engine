#include "GizmoInnerPass.h"
#include "RenderPassRegistry.h"

REGISTER_RENDER_PASS(FGizmoInnerPass)

FGizmoInnerPass::FGizmoInnerPass()
{
	PassType    = ERenderPass::GizmoInner;
	RenderState = { EDepthStencilState::GizmoInside, EBlendState::AlphaBlend,
	                ERasterizerState::SolidBackCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
}
