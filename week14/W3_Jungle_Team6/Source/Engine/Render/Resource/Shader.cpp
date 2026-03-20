#include "Shader.h"

#include <codecvt>
#include <iostream>

void FShader::Create(ID3D11Device* InDevice, const wchar_t* InFilePath, const char * InVSEntryPoint, const char * InPSEntryPoint,
		const D3D11_INPUT_ELEMENT_DESC * InInputElements, UINT InInputElementCount)
{
	ID3DBlob* vertexShaderCSO = nullptr;
	ID3DBlob* pixelShaderCSO = nullptr;
	ID3DBlob* errorBlob = nullptr;	//	For Debugging
	HRESULT hr = D3DCompileFromFile(InFilePath, nullptr, nullptr, InVSEntryPoint, "vs_5_0", 0, 0, &vertexShaderCSO, &errorBlob);
#if DEBUG
	//	Vertex Shader Compile Error
	if (FAILED(hr))
	{
		if (errorBlob)
		{
			MessageBoxA(nullptr,
				(char*)errorBlob->GetBufferPointer(),
				"Vertex Shader Compile Error",
				MB_OK | MB_ICONERROR);
			errorBlob->Release();
		}
	}
#endif
	hr = D3DCompileFromFile(InFilePath, nullptr, nullptr, InPSEntryPoint, "ps_5_0", 0, 0, &pixelShaderCSO, nullptr);

#if DEBUG
	//	Vertex Shader Compile Error
	if (FAILED(hr))
	{
		if (errorBlob)
		{
			MessageBoxA(nullptr,
				(char*)errorBlob->GetBufferPointer(),
				"Vertex Shader Compile Error",
				MB_OK | MB_ICONERROR);
			errorBlob->Release();
		}
	}
#endif
	hr = InDevice->CreateVertexShader(vertexShaderCSO->GetBufferPointer(), vertexShaderCSO->GetBufferSize(), nullptr, &VertexShader);

#if DEBUG
	//	Vertex Shader Compile Error
	if (FAILED(hr))
	{
		if (errorBlob)
		{
			MessageBoxA(nullptr,
				(char*)errorBlob->GetBufferPointer(),
				"Vertex Shader Compile Error",
				MB_OK | MB_ICONERROR);
			errorBlob->Release();
		}
	}
#endif

	hr = InDevice->CreatePixelShader(pixelShaderCSO->GetBufferPointer(), pixelShaderCSO->GetBufferSize(), nullptr, &PixelShader);

#if DEBUG
	//	Vertex Shader Compile Error
	if (FAILED(hr))
	{
		if (errorBlob)
		{
			MessageBoxA(nullptr,
				(char*)errorBlob->GetBufferPointer(),
				"Vertex Shader Compile Error",
				MB_OK | MB_ICONERROR);
			errorBlob->Release();
		}
	}
#endif

	//	Vertex Shader의 Input Layout을 생성합니다. (원래는 InInputElementCount 대신 ARRAYSIZE(InInputElements)인데, 이는 배열에서만 동작합니다.)
	InDevice->CreateInputLayout(InInputElements, InInputElementCount, vertexShaderCSO->GetBufferPointer(), vertexShaderCSO->GetBufferSize(), &InputLayout);
	
	vertexShaderCSO->Release();
	pixelShaderCSO->Release();
}

void FShader::Release()
{
	if (InputLayout)
	{
		InputLayout->Release();
		InputLayout = nullptr;
	}
	if (PixelShader)
	{
		PixelShader->Release();
		PixelShader = nullptr;
	}
	if (VertexShader)
	{
		VertexShader->Release();
		VertexShader = nullptr;
	}
}

void FShader::Bind(ID3D11DeviceContext* InDeviceContext) const
{
	InDeviceContext->IASetInputLayout(InputLayout);
	InDeviceContext->VSSetShader(VertexShader, nullptr, 0);
	InDeviceContext->PSSetShader(PixelShader, nullptr, 0);
}