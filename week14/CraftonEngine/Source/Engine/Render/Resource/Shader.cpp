#include "Shader.h"
#include "Profiling/MemoryStats.h"

#include <iostream>

FShader::FShader(FShader&& Other) noexcept
	: VertexShader(Other.VertexShader)
	, PixelShader(Other.PixelShader)
	, InputLayout(Other.InputLayout)
{
	Other.VertexShader = nullptr;
	Other.PixelShader = nullptr;
	Other.InputLayout = nullptr;
}

FShader& FShader::operator=(FShader&& Other) noexcept
{
	if (this != &Other)
	{
		Release();
		VertexShader = Other.VertexShader;
		PixelShader = Other.PixelShader;
		InputLayout = Other.InputLayout;
		Other.VertexShader = nullptr;
		Other.PixelShader = nullptr;
		Other.InputLayout = nullptr;
	}
	return *this;
}

void FShader::Create(ID3D11Device* InDevice, const wchar_t* InFilePath, const char* InVSEntryPoint, const char* InPSEntryPoint,
	const D3D11_INPUT_ELEMENT_DESC* InInputElements, UINT InInputElementCount,
	const D3D_SHADER_MACRO* InDefines)
{
	Release();

	ID3DBlob* vertexShaderCSO = nullptr;
	ID3DBlob* pixelShaderCSO = nullptr;
	ID3DBlob* errorBlob = nullptr;

	// Vertex Shader 컴파일
	HRESULT hr = D3DCompileFromFile(InFilePath, InDefines, D3D_COMPILE_STANDARD_FILE_INCLUDE, InVSEntryPoint, "vs_5_0", 0, 0, &vertexShaderCSO, &errorBlob);
	if (FAILED(hr))
	{
		if (errorBlob)
		{
			MessageBoxA(nullptr, (char*)errorBlob->GetBufferPointer(), "Vertex Shader Compile Error", MB_OK | MB_ICONERROR);
			errorBlob->Release();
		}
		return;
	}

	// Pixel Shader 컴파일
	hr = D3DCompileFromFile(InFilePath, InDefines, D3D_COMPILE_STANDARD_FILE_INCLUDE, InPSEntryPoint, "ps_5_0", 0, 0, &pixelShaderCSO, &errorBlob);
	if (FAILED(hr))
	{
		if (errorBlob)
		{
			MessageBoxA(nullptr, (char*)errorBlob->GetBufferPointer(), "Pixel Shader Compile Error", MB_OK | MB_ICONERROR);
			errorBlob->Release();
		}
		vertexShaderCSO->Release();
		return;
	}

	// Vertex Shader 생성
	hr = InDevice->CreateVertexShader(vertexShaderCSO->GetBufferPointer(), vertexShaderCSO->GetBufferSize(), nullptr, &VertexShader);
	if (FAILED(hr))
	{
		std::cerr << "Failed to create Vertex Shader (HRESULT: " << hr << ")" << std::endl;
		vertexShaderCSO->Release();
		pixelShaderCSO->Release();
		return;
	}

	CachedVertexShaderSize = vertexShaderCSO->GetBufferSize();
	MemoryStats::AddVertexShaderMemory(CachedVertexShaderSize);

	// Pixel Shader 생성
	hr = InDevice->CreatePixelShader(pixelShaderCSO->GetBufferPointer(), pixelShaderCSO->GetBufferSize(), nullptr, &PixelShader);
	if (FAILED(hr))
	{
		std::cerr << "Failed to create Pixel Shader (HRESULT: " << hr << ")" << std::endl;
		Release();
		vertexShaderCSO->Release();
		pixelShaderCSO->Release();
		return;
	}

	CachedPixelShaderSize = pixelShaderCSO->GetBufferSize();
	MemoryStats::AddPixelShaderMemory(CachedPixelShaderSize);

	// Input Layout 생성 (fullscreen quad 등 vertex buffer 없는 셰이더는 스킵)
	if (InInputElements && InInputElementCount > 0)
	{
		hr = InDevice->CreateInputLayout(InInputElements, InInputElementCount, vertexShaderCSO->GetBufferPointer(), vertexShaderCSO->GetBufferSize(), &InputLayout);
		if (FAILED(hr))
		{
			std::cerr << "Failed to create Input Layout (HRESULT: " << hr << ")" << std::endl;
			Release();
			vertexShaderCSO->Release();
			pixelShaderCSO->Release();
			return;
		}
	}

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
		MemoryStats::SubPixelShaderMemory(CachedPixelShaderSize);
		CachedPixelShaderSize = 0;

		PixelShader->Release();
		PixelShader = nullptr;
	}
	if (VertexShader)
	{
		MemoryStats::SubVertexShaderMemory(CachedVertexShaderSize);
		CachedVertexShaderSize = 0;

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
