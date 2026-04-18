#include "PassRenderStateTable.h"

void FPassRenderStateTable::Initialize()
{
	using E = ERenderPass;
	auto& S = States;

	//                              DepthStencil                         Blend                Rasterizer                   Topology                                WireframeAware
	S[(uint32)E::PreDepth]      = { EDepthStencilState::Default,         EBlendState::NoColor,    ERasterizerState::SolidBackCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, true };
	S[(uint32)E::Opaque]        = { EDepthStencilState::DepthGreaterEqual,EBlendState::Opaque,    ERasterizerState::SolidBackCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, true };
	S[(uint32)E::AlphaBlend]    = { EDepthStencilState::Default,         EBlendState::AlphaBlend, ERasterizerState::SolidBackCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, true };
	S[(uint32)E::Decal]         = { EDepthStencilState::DepthReadOnly,   EBlendState::AlphaBlend, ERasterizerState::SolidNoCull,   D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, true };
	S[(uint32)E::AdditiveDecal] = { EDepthStencilState::DepthReadOnly,   EBlendState::Additive,   ERasterizerState::SolidNoCull,   D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, true };
	S[(uint32)E::SelectionMask] = { EDepthStencilState::StencilWrite,    EBlendState::NoColor,    ERasterizerState::SolidNoCull,   D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
	S[(uint32)E::EditorLines]   = { EDepthStencilState::Default,         EBlendState::AlphaBlend, ERasterizerState::SolidBackCull, D3D11_PRIMITIVE_TOPOLOGY_LINELIST,     false };
	S[(uint32)E::PostProcess]   = { EDepthStencilState::NoDepth,         EBlendState::AlphaBlend, ERasterizerState::SolidNoCull,   D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
	S[(uint32)E::FXAA]          = { EDepthStencilState::NoDepth,         EBlendState::Opaque,     ERasterizerState::SolidNoCull,   D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
	S[(uint32)E::GizmoOuter]    = { EDepthStencilState::GizmoOutside,    EBlendState::Opaque,     ERasterizerState::SolidBackCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
	S[(uint32)E::GizmoInner]    = { EDepthStencilState::GizmoInside,     EBlendState::AlphaBlend, ERasterizerState::SolidBackCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
	S[(uint32)E::OverlayFont]   = { EDepthStencilState::NoDepth,         EBlendState::AlphaBlend, ERasterizerState::SolidBackCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
}
