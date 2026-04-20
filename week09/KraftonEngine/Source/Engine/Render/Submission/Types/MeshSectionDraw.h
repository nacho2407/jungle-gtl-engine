#pragma once

#include "Render/Types/RenderStateTypes.h"

class FConstantBuffer;
struct ID3D11ShaderResourceView;

struct FMeshSectionDraw
{
    ID3D11ShaderResourceView* DiffuseSRV = nullptr;
    ID3D11ShaderResourceView* NormalSRV = nullptr;
    uint32 FirstIndex = 0;
    uint32 IndexCount = 0;

    EBlendState Blend = EBlendState::Opaque;
    EDepthStencilState DepthStencil = EDepthStencilState::Default;
    ERasterizerState Rasterizer = ERasterizerState::SolidBackCull;

    FConstantBuffer* MaterialCB[2] = {};
};
