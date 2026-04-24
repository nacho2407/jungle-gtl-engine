// 렌더 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Render/Resources/State/RenderStateTypes.h"

class FConstantBuffer;
struct ID3D11ShaderResourceView;

// FMeshSectionRenderData는 메시 데이터와 렌더 제출 정보를 다룹니다.
struct FMeshSectionRenderData
{
    ID3D11ShaderResourceView* DiffuseSRV  = nullptr;
    ID3D11ShaderResourceView* NormalSRV   = nullptr;
    ID3D11ShaderResourceView* SpecularSRV = nullptr;
    uint32                    FirstIndex  = 0;
    uint32                    IndexCount  = 0;

    EBlendState        Blend        = EBlendState::Opaque;
    EDepthStencilState DepthStencil = EDepthStencilState::Default;
    ERasterizerState   Rasterizer   = ERasterizerState::SolidBackCull;

    FConstantBuffer* MaterialCB[2] = {};
};
