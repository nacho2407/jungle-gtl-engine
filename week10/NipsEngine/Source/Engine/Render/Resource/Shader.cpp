#include "Shader.h"

#include <iostream>

void FShader::Create(ID3D11Device* InDevice, const wchar_t* InFilePath, const char * InVSEntryPoint, const char * InPSEntryPoint,
		const D3D11_INPUT_ELEMENT_DESC * InInputElements, UINT InInputElementCount)
{
	TComPtr<ID3DBlob> vertexShaderCSO;
	TComPtr<ID3DBlob> pixelShaderCSO;
	TComPtr<ID3DBlob> errorBlob;

	// Vertex Shader 컴파일
	HRESULT hr = D3DCompileFromFile(InFilePath, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, InVSEntryPoint, "vs_5_0", 0, 0,
		vertexShaderCSO.GetAddressOf(), errorBlob.GetAddressOf());
	if (FAILED(hr))
	{
		if (errorBlob)
		{
			MessageBoxA(nullptr, (char*)errorBlob->GetBufferPointer(), "Vertex Shader Compile Error", MB_OK | MB_ICONERROR);
		}
		return;
	}

	// Pixel Shader 컴파일
	errorBlob.Reset();
	hr = D3DCompileFromFile(InFilePath, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, InPSEntryPoint, "ps_5_0", 0, 0,
		pixelShaderCSO.GetAddressOf(), errorBlob.GetAddressOf());
	if (FAILED(hr))
	{
		if (errorBlob)
		{
			MessageBoxA(nullptr, (char*)errorBlob->GetBufferPointer(), "Pixel Shader Compile Error", MB_OK | MB_ICONERROR);
		}
		return;
	}

	// Vertex Shader 생성
	hr = InDevice->CreateVertexShader(vertexShaderCSO->GetBufferPointer(), vertexShaderCSO->GetBufferSize(), nullptr,
		VertexShader.ReleaseAndGetAddressOf());
	if (FAILED(hr))
	{
		std::cerr << "Failed to create Vertex Shader (HRESULT: " << hr << ")" << std::endl;
		return;
	}

	// Pixel Shader 생성
	hr = InDevice->CreatePixelShader(pixelShaderCSO->GetBufferPointer(), pixelShaderCSO->GetBufferSize(), nullptr,
		PixelShader.ReleaseAndGetAddressOf());
	if (FAILED(hr))
	{
		std::cerr << "Failed to create Pixel Shader (HRESULT: " << hr << ")" << std::endl;
		return;
	}

	// Input Layout 생성 (fullscreen triangle 등 입력 레이아웃이 없는 VS 지원)
	if (InInputElements != nullptr && InInputElementCount > 0)
	{
		hr = InDevice->CreateInputLayout(InInputElements, InInputElementCount, vertexShaderCSO->GetBufferPointer(),
			vertexShaderCSO->GetBufferSize(), InputLayout.ReleaseAndGetAddressOf());
		if (FAILED(hr))
		{
			std::cerr << "Failed to create Input Layout (HRESULT: " << hr << ")" << std::endl;
			return;
		}
	}
	else
	{
		InputLayout.Reset();
	}
}

void FShader::Release()
{
	InputLayout.Reset();
	PixelShader.Reset();
	VertexShader.Reset();
}

void FShader::Bind(ID3D11DeviceContext* InDeviceContext) const
{
	InDeviceContext->IASetInputLayout(InputLayout.Get());
	InDeviceContext->VSSetShader(VertexShader.Get(), nullptr, 0);
	InDeviceContext->PSSetShader(PixelShader.Get(), nullptr, 0);
}
