#pragma once

#include <d3d11.h>
#include "EngineCore/EngineTypes.h"
#include "ConstantBuffer.h"

struct FMaterial
{
	TComPtr<ID3D11VertexShader> VertexShader = nullptr;
	TComPtr<ID3D11InputLayout> InputLayout = nullptr;
	TComPtr<ID3D11PixelShader> PixelShader = nullptr;
	FConstantBuffer ConstantBuffer;

	// for texture
	TComPtr<ID3D11ShaderResourceView> ShaderResourceView = nullptr;
	TComPtr<ID3D11SamplerState> SamplerState = nullptr;
	bool bUseTexture = false;

	FMaterial()
	{
	}
	~FMaterial()
	{
		VertexShader.Reset();
		InputLayout.Reset();
		PixelShader.Reset();
	}

	void Bind(ID3D11DeviceContext* deviceContext)
	{
		deviceContext->IASetInputLayout(InputLayout.Get());
		deviceContext->VSSetShader(VertexShader.Get(), nullptr, 0);
		deviceContext->PSSetShader(PixelShader.Get(), nullptr, 0);

		// for texture
		if (bUseTexture)
		{
			deviceContext->PSSetShaderResources(0, 1, ShaderResourceView.GetAddressOf());
			deviceContext->PSSetSamplers(0, 1, SamplerState.GetAddressOf());
		}
	}
};