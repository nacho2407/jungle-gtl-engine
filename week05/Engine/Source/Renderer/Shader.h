#pragma once

#include "EngineAPI.h"
#include <d3d11.h>
#include <memory>

class FShaderResource;

class ENGINE_API FVertexShader
{
public:
	~FVertexShader();

	static std::shared_ptr<FVertexShader> Create(
		ID3D11Device* Device,
		const std::shared_ptr<FShaderResource>& Resource
	);

	static std::shared_ptr<FVertexShader> CreateWithLayout(
		ID3D11Device* Device,
		const std::shared_ptr<FShaderResource>& Resource,
		const D3D11_INPUT_ELEMENT_DESC* Layout,
		UINT LayoutCount
	);

	void Bind(ID3D11DeviceContext* DeviceContext) const;
	void Release();

private:
	FVertexShader() = default;

	ID3D11VertexShader* Shader = nullptr;
	ID3D11InputLayout* InputLayout = nullptr;
};

class ENGINE_API FPixelShader
{
public:
	~FPixelShader();

	static std::shared_ptr<FPixelShader> Create(
		ID3D11Device* Device,
		const std::shared_ptr<FShaderResource>& Resource
	);

	void Bind(ID3D11DeviceContext* DeviceContext) const;
	void Release();

private:
	FPixelShader() = default;

	ID3D11PixelShader* Shader = nullptr;
};
