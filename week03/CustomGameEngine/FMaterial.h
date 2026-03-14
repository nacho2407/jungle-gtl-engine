#pragma once

#include "d3d11.h"

struct FMaterial
{
	ID3D11VertexShader* VertexShader = nullptr;
	ID3D11InputLayout* InputLayout = nullptr;
	ID3D11PixelShader* PixelShader = nullptr;

	void BindShaders(ID3D11DeviceContext* deviceContext)
	{
		deviceContext->IASetInputLayout(InputLayout);
		deviceContext->VSSetShader(VertexShader, nullptr, 0);
		deviceContext->PSSetShader(PixelShader, nullptr, 0);
	}
};