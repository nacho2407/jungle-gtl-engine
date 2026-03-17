#pragma once

#include <d3d11.h>
#include "EngineTypes.h"

struct FMaterial
{
	TComPtr<ID3D11VertexShader> VertexShader = nullptr;
	TComPtr<ID3D11InputLayout> InputLayout = nullptr;
	TComPtr<ID3D11PixelShader> PixelShader = nullptr;

	FMaterial()
	{
	}
	~FMaterial()
	{
		VertexShader.Reset();
		InputLayout.Reset();
		PixelShader.Reset();
	}

	void BindShaders(ID3D11DeviceContext* deviceContext)
	{
		deviceContext->IASetInputLayout(InputLayout.Get());
		deviceContext->VSSetShader(VertexShader.Get(), nullptr, 0);
		deviceContext->PSSetShader(PixelShader.Get(), nullptr, 0);
	}
};