#pragma once
#include <d3d11.h>
#include "Core/CoreMinimal.h"

struct FPassRenderState;
struct FRenderTargetSet;
struct FRenderResources;
class FRenderBus;
class FFontBatcher;
class FSubUVBatcher;
class FLineBatcher;

struct FRenderPassContext
{
    const FRenderBus* RenderBus = nullptr;
    const FRenderTargetSet* RenderTargets = nullptr;
    const FPassRenderState* RenderState = nullptr;
    ID3D11Device* Device = nullptr;
    ID3D11DeviceContext* DeviceContext = nullptr;
    FRenderResources* RenderResources = nullptr;
    FFontBatcher* FontBatcher = nullptr;
    FSubUVBatcher* SubUVBatcher = nullptr;
    FLineBatcher* GridLineBatcher = nullptr;
    FLineBatcher* EditorLineBatcher = nullptr;

	ID3D11RenderTargetView* FinalRTV = nullptr;
    ID3D11ShaderResourceView* FinalSRV = nullptr;
};
