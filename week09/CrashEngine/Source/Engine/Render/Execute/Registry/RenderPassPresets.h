// 렌더 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Render/Resources/State/RenderStateTypes.h"
#include "Render/Execute/Registry/RenderPassTypes.h"
#include "Render/RHI/D3D11/Common/D3D11API.h"

// Reserved for fixed render-target or transient resource policy per pass.
struct FRenderPassResourcePreset
{
};

// Describes fixed resource binding policy for a render pass.
struct FRenderPassBindingPreset
{
};

// Describes fixed pipeline state used while drawing a render pass.
struct FRenderPassDrawPreset
{
    EDepthStencilState       DepthStencil = EDepthStencilState::Default;
    EBlendState              Blend        = EBlendState::Opaque;
    ERasterizerState         Rasterizer   = ERasterizerState::SolidBackCull;
    D3D11_PRIMITIVE_TOPOLOGY Topology     = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
};

// Describes how one render pass should acquire resources, bind them, and draw.
struct FRenderPassPreset
{
    FRenderPassResourcePreset Resource;
    FRenderPassBindingPreset  Binding;
    FRenderPassDrawPreset     Draw;
};

// Fills the default execution presets for every ERenderPass value.
void InitializeDefaultRenderPassPresets(FRenderPassPreset (&OutPresets)[(uint32)ERenderPass::MAX]);
