#include "ShaderMap.h"
#include "Shader.h"
#include "ShaderResource.h"
#include <cstring>

FShaderMap& FShaderMap::Get()
{
	static FShaderMap Instance;
	return Instance;
}

std::shared_ptr<FVertexShader> FShaderMap::GetOrCreateVertexShader(
	ID3D11Device* Device,
	const wchar_t* FilePath)
{
	std::wstring Key(FilePath);

	auto It = VertexShaders.find(Key);
	if (It != VertexShaders.end())
	{
		return It->second;
	}

	auto Resource = FShaderResource::GetOrCompile(FilePath, "main", "vs_5_0");
	if (!Resource)
	{
		return nullptr;
	}

	auto VS = FVertexShader::Create(Device, Resource);
	if (!VS)
	{
		return nullptr;
	}

	VertexShaders[Key] = VS;
	return VS;
}

std::shared_ptr<FPixelShader> FShaderMap::GetOrCreatePixelShader(
	ID3D11Device* Device,
	const wchar_t* FilePath)
{
	std::wstring Key(FilePath);

	auto It = PixelShaders.find(Key);
	if (It != PixelShaders.end())
	{
		return It->second;
	}

	auto Resource = FShaderResource::GetOrCompile(FilePath, "main", "ps_5_0");
	if (!Resource)
	{
		return nullptr;
	}

	auto PS = FPixelShader::Create(Device, Resource);
	if (!PS)
	{
		return nullptr;
	}

	PixelShaders[Key] = PS;
	return PS;
}

std::shared_ptr<FComputeShader> FShaderMap::GetOrCreateComputeShader(
	ID3D11Device* Device,
	const wchar_t* FilePath,
	const char* EntryPoint)
{
	std::wstring Key = std::wstring(FilePath) + L"|" + std::wstring(EntryPoint, EntryPoint + strlen(EntryPoint));

	auto It = ComputeShaders.find(Key);
	if (It != ComputeShaders.end())
	{
		return It->second;
	}

	auto Resource = FShaderResource::GetOrCompile(FilePath, EntryPoint, "cs_5_0");
	if (!Resource)
	{
		return nullptr;
	}

	auto CS = FComputeShader::Create(Device, Resource);
	if (!CS)
	{
		return nullptr;
	}

	ComputeShaders[Key] = CS;
	return CS;
}

void FShaderMap::Clear()
{
	VertexShaders.clear();
	PixelShaders.clear();
	ComputeShaders.clear();
	FShaderResource::ClearCache();
}
