#pragma once

#include "Core/Containers/Map.h"
#include "Core/CoreTypes.h"
#include "Render/Common/ComPtr.h"
#include "Render/Resource/RenderResources.h"
#include "Render/Resource/Material.h"

#include <d3d11.h>

struct FMaterialBlendStateDescHasher
{
	size_t operator()(const FMaterialBlendStateDesc& Desc) const
	{
		size_t Hash = std::hash<bool>{}(Desc.bBlendEnable);
		Hash ^= std::hash<uint8>{}(static_cast<uint8>(Desc.SrcColor)) + 0x9e3779b9 + (Hash << 6) + (Hash >> 2);
		Hash ^= std::hash<uint8>{}(static_cast<uint8>(Desc.DestColor)) + 0x9e3779b9 + (Hash << 6) + (Hash >> 2);
		Hash ^= std::hash<uint8>{}(static_cast<uint8>(Desc.ColorOp)) + 0x9e3779b9 + (Hash << 6) + (Hash >> 2);
		Hash ^= std::hash<uint8>{}(static_cast<uint8>(Desc.SrcAlpha)) + 0x9e3779b9 + (Hash << 6) + (Hash >> 2);
		Hash ^= std::hash<uint8>{}(static_cast<uint8>(Desc.DestAlpha)) + 0x9e3779b9 + (Hash << 6) + (Hash >> 2);
		Hash ^= std::hash<uint8>{}(static_cast<uint8>(Desc.AlphaOp)) + 0x9e3779b9 + (Hash << 6) + (Hash >> 2);
		Hash ^= std::hash<uint8>{}(Desc.WriteMask) + 0x9e3779b9 + (Hash << 6) + (Hash >> 2);
		return Hash;
	}
};

class FRenderStateResourceCache
{
public:
	ID3D11SamplerState* GetOrCreateSamplerState(ESamplerType Type, ID3D11Device* Device);
	ID3D11DepthStencilState* GetOrCreateDepthStencilState(EDepthStencilType Type, ID3D11Device* Device);
	ID3D11BlendState* GetOrCreateBlendState(EBlendType Type, ID3D11Device* Device);
	ID3D11BlendState* GetOrCreateBlendState(const FMaterialBlendStateDesc& Desc, ID3D11Device* Device);
	ID3D11RasterizerState* GetOrCreateRasterizerState(ERasterizerType Type, ID3D11Device* Device);
	void Release();

private:
	TMap<ESamplerType, TComPtr<ID3D11SamplerState>> SamplerStates;
	TMap<EDepthStencilType, TComPtr<ID3D11DepthStencilState>> DepthStencilStates;
	TMap<EBlendType, TComPtr<ID3D11BlendState>> BlendStates;
	TMap<FMaterialBlendStateDesc, TComPtr<ID3D11BlendState>, FMaterialBlendStateDescHasher> MaterialBlendStates;
	TMap<ERasterizerType, TComPtr<ID3D11RasterizerState>> RasterizerStates;
};
