#include "OverlayFontPass.h"
#include "RenderPassRegistry.h"

REGISTER_RENDER_PASS(FOverlayFontPass)

FOverlayFontPass::FOverlayFontPass()
{
	PassType    = ERenderPass::OverlayFont;
	RenderState = { EDepthStencilState::NoDepth, EBlendState::AlphaBlend,
	                ERasterizerState::SolidBackCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
}
