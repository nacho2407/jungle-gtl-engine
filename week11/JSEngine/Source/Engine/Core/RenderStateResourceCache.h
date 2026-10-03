#pragma once

#include "Core/Containers/Map.h"
#include "Core/CoreTypes.h"
#include "Render/Common/ComPtr.h"
#include "Render/Resource/RenderResources.h"

#include <d3d11.h>

class FRenderStateResourceCache
{
public:
	ID3D11SamplerState* GetOrCreateSamplerState(ESamplerType Type, ID3D11Device* Device);
	ID3D11DepthStencilState* GetOrCreateDepthStencilState(EDepthStencilType Type, ID3D11Device* Device);
	ID3D11BlendState* GetOrCreateBlendState(EBlendType Type, ID3D11Device* Device);
	ID3D11RasterizerState* GetOrCreateRasterizerState(ERasterizerType Type, ID3D11Device* Device);
	void Release();

private:
	TMap<ESamplerType, TComPtr<ID3D11SamplerState>> SamplerStates;
	TMap<EDepthStencilType, TComPtr<ID3D11DepthStencilState>> DepthStencilStates;
	TMap<EBlendType, TComPtr<ID3D11BlendState>> BlendStates;
	TMap<ERasterizerType, TComPtr<ID3D11RasterizerState>> RasterizerStates;
};
