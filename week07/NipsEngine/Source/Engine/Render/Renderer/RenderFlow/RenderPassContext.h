#pragma once
#include <d3d11.h>

struct FRenderTargetSet;
class FRenderBus;

struct FRenderPassContext
{
    const FRenderBus* RenderBus;
    const FRenderTargetSet* RenderTargets;
    ID3D11Device* Device;
    ID3D11DeviceContext* DeviceContext;
};
