#pragma once

#include "Core/Singleton.h"
#include "Render/Common/ComPtr.h"
#include <d3d11.h>

class FShadowAtlasManager : public TSingleton<FShadowAtlasManager>
{
	friend class TSingleton<FShadowAtlasManager>;
public:
	void Initialize(ID3D11Device* InDevice);
    void VSMInitialize(ID3D11Device* Device);
	
	
	TComPtr<ID3D11Device> Device;

	TComPtr<ID3D11Texture2D> ShadowMap;
	TComPtr<ID3D11DepthStencilView> ShadowDSV;
	TComPtr<ID3D11ShaderResourceView> ShadowSRV;
	
	
	TComPtr<ID3D11Texture2D> VarianceRTVShadowMap;          // 텍스처
    TComPtr<ID3D11RenderTargetView> VarianceShadowRTV;   // 쓰기용 (Shadow Pass)
    TComPtr<ID3D11ShaderResourceView> VarianceShadowSRV; // 읽기용 (Lighting Pass)

	// TComPtr<ID3D11Texture2D> VarianceDepthShadowMap;	 // DSV용 깊이 테스트
    // TComPtr<ID3D11DepthStencilView> VarianceShadowDSV;   // 깊이 테스트용 (별도)

	float ClearColor[4] = { 0.f, 0.f, 0.f, 0.f };
};
