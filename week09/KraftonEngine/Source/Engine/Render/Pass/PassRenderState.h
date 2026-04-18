#pragma once

#include "Render/Types/RenderTypes.h"

// 패스별 기본 렌더 상태 — Single Source of Truth
struct FPassRenderState
{
	EDepthStencilState       DepthStencil = EDepthStencilState::Default;
	EBlendState              Blend = EBlendState::Opaque;
	ERasterizerState         Rasterizer = ERasterizerState::SolidBackCull;
	D3D11_PRIMITIVE_TOPOLOGY Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	bool                     bWireframeAware = false;
};

void InitializeDefaultPassRenderStates(FPassRenderState (&OutStates)[(uint32)ERenderPass::MAX]);
