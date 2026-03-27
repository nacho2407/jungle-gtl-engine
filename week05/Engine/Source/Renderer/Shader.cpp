#include "Shader.h"
#include "ShaderResource.h"
#include "PrimitiveVertex.h"

// ─── FVertexShader ───

FVertexShader::~FVertexShader()
{
	Release();
}

std::shared_ptr<FVertexShader> FVertexShader::Create(
	ID3D11Device* Device,
	const std::shared_ptr<FShaderResource>& Resource)
{
	if (!Device || !Resource || !Resource->GetBufferPointer())
	{
		return nullptr;
	}

	std::shared_ptr<FVertexShader> VS(new FVertexShader());

	HRESULT Hr = Device->CreateVertexShader(
		Resource->GetBufferPointer(),
		Resource->GetBufferSize(),
		nullptr, &VS->Shader
	);

	if (FAILED(Hr))
	{
		return nullptr;
	}

	D3D11_INPUT_ELEMENT_DESC Layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,                            D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT,  0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,     0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,        0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	Hr = Device->CreateInputLayout(
		Layout, 4,
		Resource->GetBufferPointer(),
		Resource->GetBufferSize(),
		&VS->InputLayout
	);

	if (FAILED(Hr))
	{
		VS->Release();
		return nullptr;
	}

	return VS;
}

void FVertexShader::Bind(ID3D11DeviceContext* DeviceContext) const
{
	DeviceContext->IASetInputLayout(InputLayout);
	DeviceContext->VSSetShader(Shader, nullptr, 0);
}

void FVertexShader::Release()
{
	if (InputLayout) { InputLayout->Release(); InputLayout = nullptr; }
	if (Shader) { Shader->Release(); Shader = nullptr; }
}

// ─── FPixelShader ───

FPixelShader::~FPixelShader()
{
	Release();
}

std::shared_ptr<FPixelShader> FPixelShader::Create(
	ID3D11Device* Device,
	const std::shared_ptr<FShaderResource>& Resource)
{
	if (!Device || !Resource || !Resource->GetBufferPointer())
	{
		return nullptr;
	}

	std::shared_ptr<FPixelShader> PS(new FPixelShader());

	HRESULT Hr = Device->CreatePixelShader(
		Resource->GetBufferPointer(),
		Resource->GetBufferSize(),
		nullptr, &PS->Shader
	);

	if (FAILED(Hr))
	{
		return nullptr;
	}

	return PS;
}

std::shared_ptr<FVertexShader> FVertexShader::CreateWithLayout(
	ID3D11Device* Device,
	const std::shared_ptr<FShaderResource>& Resource,
	const D3D11_INPUT_ELEMENT_DESC* Layout,
	UINT LayoutCount)
{
	if (!Device || !Resource || !Resource->GetBufferPointer() || !Layout || LayoutCount == 0)
	{
		return nullptr;
	}

	std::shared_ptr<FVertexShader> VS(new FVertexShader());

	HRESULT Hr = Device->CreateVertexShader(
		Resource->GetBufferPointer(),
		Resource->GetBufferSize(),
		nullptr,
		&VS->Shader
	);

	if (FAILED(Hr))
	{
		return nullptr;
	}

	Hr = Device->CreateInputLayout(
		Layout,
		LayoutCount,
		Resource->GetBufferPointer(),
		Resource->GetBufferSize(),
		&VS->InputLayout
	);

	if (FAILED(Hr))
	{
		VS->Release();
		return nullptr;
	}

	return VS;
}

void FPixelShader::Bind(ID3D11DeviceContext* DeviceContext) const
{
	DeviceContext->PSSetShader(Shader, nullptr, 0);
}

void FPixelShader::Release()
{
	if (Shader) { Shader->Release(); Shader = nullptr; }
}
