#include "Render/Passes/Base/PassRenderState.h"

void InitializeDefaultPassRenderStateDescs(FPassRenderStateDesc (&OutStateDescs)[(uint32)ERenderPass::MAX])
{
    using E = ERenderPass;
    auto& S = OutStateDescs;

    S[(uint32)E::Grid] = { EDepthStencilState::DepthReadOnly, EBlendState::AlphaBlend, ERasterizerState::SolidBackCull, D3D11_PRIMITIVE_TOPOLOGY_LINELIST };
    S[(uint32)E::DepthPre] = { EDepthStencilState::Default, EBlendState::NoColor, ERasterizerState::SolidBackCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST };
    S[(uint32)E::Opaque] = { EDepthStencilState::Default, EBlendState::Opaque, ERasterizerState::SolidBackCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST };
    S[(uint32)E::Decal] = { EDepthStencilState::DepthReadOnly, EBlendState::AlphaBlend, ERasterizerState::SolidNoCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST };
    S[(uint32)E::Lighting] = { EDepthStencilState::NoDepth, EBlendState::Opaque, ERasterizerState::SolidNoCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST };
    S[(uint32)E::AdditiveDecal] = { EDepthStencilState::DepthReadOnly, EBlendState::Additive, ERasterizerState::SolidNoCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST };
    S[(uint32)E::AlphaBlend] = { EDepthStencilState::Default, EBlendState::AlphaBlend, ERasterizerState::SolidBackCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST };
    S[(uint32)E::SelectionMask] = { EDepthStencilState::StencilWrite, EBlendState::NoColor, ERasterizerState::SolidNoCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST };
    S[(uint32)E::EditorLines] = { EDepthStencilState::DepthReadOnly, EBlendState::AlphaBlend, ERasterizerState::SolidBackCull, D3D11_PRIMITIVE_TOPOLOGY_LINELIST };
    S[(uint32)E::PostProcess] = { EDepthStencilState::NoDepth, EBlendState::AlphaBlend, ERasterizerState::SolidNoCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST };
    S[(uint32)E::FXAA] = { EDepthStencilState::NoDepth, EBlendState::Opaque, ERasterizerState::SolidNoCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST };
    S[(uint32)E::GizmoOuter] = { EDepthStencilState::GizmoOutside, EBlendState::Opaque, ERasterizerState::SolidBackCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST };
    S[(uint32)E::GizmoInner] = { EDepthStencilState::GizmoInside, EBlendState::AlphaBlend, ERasterizerState::SolidBackCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST };
    S[(uint32)E::OverlayFont] = { EDepthStencilState::NoDepth, EBlendState::AlphaBlend, ERasterizerState::SolidBackCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST };
}
