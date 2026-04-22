#pragma once

#include "Render/Execute/Context/PipelineStateTypes.h"
#include "Render/Execute/Passes/Base/RenderPassTypes.h"
#include "Render/RHI/D3D11/Common/D3D11API.h"

/*
    �??�더 ?�스가 기본?�으�??�용???�태 기술?�입?�다.
    ?�스�?기본 Depth / Blend / Rasterizer / Topology 조합????군데?�서 ?�의?�니??
*/
struct FPassRenderStateDesc
{
    EDepthStencilState DepthStencil;
    EBlendState Blend;
    ERasterizerState Rasterizer;
    D3D11_PRIMITIVE_TOPOLOGY Topology;
};

void InitializeDefaultPassRenderStateDescs(FPassRenderStateDesc (&OutStateDescs)[(uint32)ERenderPass::MAX]);
