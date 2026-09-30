// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/Execute/Registry/RenderPassPresets.h"

// ========== Default Presets ==========

void InitializeDefaultRenderPassPresets(FRenderPassPreset (&OutPresets)[(uint32)ERenderPass::MAX])
{
    using E = ERenderPass;
    auto& P = OutPresets;

    // ---------- Scene ----------
    P[(uint32)E::Grid].Draw          = { EDepthStencilState::DepthReadOnly, EBlendState::AlphaBlend, ERasterizerState::SolidBackCull, D3D11_PRIMITIVE_TOPOLOGY_LINELIST };
    P[(uint32)E::DepthPre].Draw      = { EDepthStencilState::Default, EBlendState::NoColor, ERasterizerState::SolidBackCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST };
    P[(uint32)E::ShadowMap].Draw     = { EDepthStencilState::Default, EBlendState::Opaque, ERasterizerState::SolidFrontCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST };
    P[(uint32)E::Opaque].Draw        = { EDepthStencilState::Default, EBlendState::Opaque, ERasterizerState::SolidBackCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST };
    P[(uint32)E::Decal].Draw         = { EDepthStencilState::NoDepth, EBlendState::Opaque, ERasterizerState::SolidNoCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST };
    P[(uint32)E::DeferredLighting].Draw = { EDepthStencilState::NoDepth, EBlendState::Opaque, ERasterizerState::SolidNoCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST };
    P[(uint32)E::AdditiveDecal].Draw = { EDepthStencilState::DepthReadOnly, EBlendState::Additive, ERasterizerState::SolidNoCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST };
    P[(uint32)E::AlphaBlend].Draw    = { EDepthStencilState::Default, EBlendState::AlphaBlend, ERasterizerState::SolidBackCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST };

    // ---------- Editor And Post Process ----------
    P[(uint32)E::SelectionMask].Draw    = { EDepthStencilState::StencilWrite, EBlendState::NoColor, ERasterizerState::SolidNoCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST };
    P[(uint32)E::EditorLines].Draw      = { EDepthStencilState::DepthReadOnly, EBlendState::AlphaBlend, ERasterizerState::SolidBackCull, D3D11_PRIMITIVE_TOPOLOGY_LINELIST };
    P[(uint32)E::PostProcess].Draw      = { EDepthStencilState::NoDepth, EBlendState::AlphaBlend, ERasterizerState::SolidNoCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST };
    P[(uint32)E::FXAA].Draw             = { EDepthStencilState::NoDepth, EBlendState::Opaque, ERasterizerState::SolidNoCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST };
    P[(uint32)E::GizmoOuter].Draw       = { EDepthStencilState::GizmoOutside, EBlendState::Opaque, ERasterizerState::SolidBackCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST };
    P[(uint32)E::GizmoInner].Draw       = { EDepthStencilState::GizmoInside, EBlendState::AlphaBlend, ERasterizerState::SolidBackCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST };
    P[(uint32)E::OverlayBillboard].Draw = { EDepthStencilState::NoDepth, EBlendState::AlphaBlend, ERasterizerState::SolidNoCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST };
    P[(uint32)E::OverlayTextWorld].Draw = { EDepthStencilState::NoDepth, EBlendState::AlphaBlend, ERasterizerState::SolidNoCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST };
    P[(uint32)E::OverlayFont].Draw      = { EDepthStencilState::NoDepth, EBlendState::AlphaBlend, ERasterizerState::SolidBackCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST };
}
