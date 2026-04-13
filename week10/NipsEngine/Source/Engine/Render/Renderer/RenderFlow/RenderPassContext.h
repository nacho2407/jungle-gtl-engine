#pragma once
#include <d3d11.h>

struct FPassRenderState;
struct FRenderTargetSet;
struct FRenderResources;
class FRenderBus;

struct FRenderPassContext
{
    const FRenderBus* RenderBus;
    const FRenderTargetSet* RenderTargets;
    const FPassRenderState* RenderState;
    ID3D11Device* Device;
    ID3D11DeviceContext* DeviceContext;
    FRenderResources* RenderResources;
};
