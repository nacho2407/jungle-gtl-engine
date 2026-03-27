#pragma once

#include "EngineAPI.h"
#include <d3d11.h>
#include <string>
#include <unordered_map>
#include <memory>

class FVertexShader;
class FPixelShader;

class ENGINE_API FShaderMap
{
public:
	std::shared_ptr<FVertexShader> GetOrCreateVertexShader(
		ID3D11Device* Device,
		const wchar_t* FilePath
	);

	std::shared_ptr<FPixelShader> GetOrCreatePixelShader(
		ID3D11Device* Device,
		const wchar_t* FilePath
	);

	void Clear();

	static FShaderMap& Get();

private:
	FShaderMap() = default;

	std::unordered_map<std::wstring, std::shared_ptr<FVertexShader>> VertexShaders;
	std::unordered_map<std::wstring, std::shared_ptr<FPixelShader>> PixelShaders;
};
