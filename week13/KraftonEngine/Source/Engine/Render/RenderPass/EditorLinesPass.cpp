#include "EditorLinesPass.h"
#include "RenderPassRegistry.h"

REGISTER_RENDER_PASS(FEditorLinesPass)

FEditorLinesPass::FEditorLinesPass()
{
	PassType = ERenderPass::EditorLines;
	RenderState = { EDepthStencilState::DepthReadOnly, EBlendState::AlphaBlend,
	                ERasterizerState::SolidBackCull, D3D11_PRIMITIVE_TOPOLOGY_LINELIST, false };
}
