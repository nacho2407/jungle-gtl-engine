#pragma once

#include <d3d11.h>
#include "EngineCore/EngineTypes.h"
#include "ConstantBuffer.h"

struct FMaterial
{
	TComPtr<ID3D11VertexShader> VertexShader = nullptr;
	TComPtr<ID3D11InputLayout> InputLayout = nullptr;
	TComPtr<ID3D11PixelShader> PixelShader = nullptr;
	TComPtr<ID3D11ShaderResourceView> TextureSRV = nullptr;
	TComPtr<ID3D11SamplerState> SamplerState = nullptr;
	FConstantBuffer ConstantBuffer;
	FConstantBuffer SecondaryConstantBuffer;
	bool bUsesTexture = false;
	bool bUsesSecondaryConstantBuffer = false;

	FMaterial()
	{
	}
	~FMaterial()
	{
		VertexShader.Reset();
		InputLayout.Reset();
		PixelShader.Reset();
		TextureSRV.Reset();
		SamplerState.Reset();
		SecondaryConstantBuffer.Release();
	}

	void Bind(ID3D11DeviceContext* deviceContext)
	{
		deviceContext->IASetInputLayout(InputLayout.Get());
		deviceContext->VSSetShader(VertexShader.Get(), nullptr, 0);
		deviceContext->PSSetShader(PixelShader.Get(), nullptr, 0);

		ID3D11ShaderResourceView* shaderResourceView = bUsesTexture ? TextureSRV.Get() : nullptr;
		deviceContext->PSSetShaderResources(0, 1, &shaderResourceView);

		ID3D11SamplerState* samplerState = bUsesTexture ? SamplerState.Get() : nullptr;
		deviceContext->PSSetSamplers(0, 1, &samplerState);
	}
};
