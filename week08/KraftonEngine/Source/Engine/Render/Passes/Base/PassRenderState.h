#pragma once

#include "Render/Passes/Base/PipelineStateTypes.h"
#include "Render/Passes/Base/RenderPassTypes.h"
#include "Render/RHI/D3D11/Common/D3D11API.h"

/*
    각 렌더 패스가 기본적으로 사용할 상태 기술서입니다.
    패스별 기본 Depth / Blend / Rasterizer / Topology 조합을 한 군데에서 정의합니다.
*/
struct FPassRenderStateDesc
{
    EDepthStencilState DepthStencil;
    EBlendState Blend;
    ERasterizerState Rasterizer;
    D3D11_PRIMITIVE_TOPOLOGY Topology;
};

void InitializeDefaultPassRenderStateDescs(FPassRenderStateDesc (&OutStateDescs)[(uint32)ERenderPass::MAX]);
