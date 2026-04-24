#pragma once

#include "Core/Singleton.h"
#include "Render/Common/ComPtr.h"
#include <d3d11.h>

class FShadowAtlasManager : public TSingleton<FShadowAtlasManager>
{
	friend class TSingleton<FShadowAtlasManager>;
public:
	void Initialize(ID3D11Device* InDevice);

	TComPtr<ID3D11Device> Device;

	TComPtr<ID3D11Texture2D> ShadowMap;
	TComPtr<ID3D11DepthStencilView> ShadowDSV;
	TComPtr<ID3D11ShaderResourceView> ShadowSRV;
};
