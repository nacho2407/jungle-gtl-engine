#pragma once

#include "Render/Types/RenderTypes.h"
#include "Render/Types/RenderStateTypes.h"
#include "Render/Types/ViewTypes.h"
#include "DrawCommand.h"

/*
	패스별 기본 렌더 상태 — Single Source of Truth
	패스 enum → 렌더 상태 매핑 테이블을 소유하며,
	Wireframe 오버라이드를 포함한 FDrawCommandRenderState 변환을 제공합니다.
*/

struct FPassRenderState
{
	EDepthStencilState       DepthStencil = EDepthStencilState::Default;
	EBlendState              Blend = EBlendState::Opaque;
	ERasterizerState         Rasterizer = ERasterizerState::SolidBackCull;
	D3D11_PRIMITIVE_TOPOLOGY Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	bool                     bWireframeAware = false;
};

class FPassRenderStateTable
{
public:
	void Initialize();

	// 패스별 원본 렌더 상태 조회
	const FPassRenderState& Get(ERenderPass Pass) const
	{
		return States[(uint32)Pass];
	}

	// FDrawCommandRenderState로 변환 (Wireframe 오버라이드 포함)
	FDrawCommandRenderState ToDrawCommandState(ERenderPass Pass, EViewMode ViewMode) const
	{
		const FPassRenderState& S = States[(uint32)Pass];
		FDrawCommandRenderState RS;
		RS.DepthStencil = S.DepthStencil;
		RS.Blend        = S.Blend;
		RS.Rasterizer   = (S.bWireframeAware && ViewMode == EViewMode::Wireframe)
			? ERasterizerState::WireFrame : S.Rasterizer;
		RS.Topology     = S.Topology;
		return RS;
	}

private:
	FPassRenderState States[(uint32)ERenderPass::MAX] = {};
};
