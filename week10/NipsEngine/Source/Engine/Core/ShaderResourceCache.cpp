#include "Core/ShaderResourceCache.h"

#include "Core/Paths.h"
#include "Core/Logging/Log.h"
#include "Object/ObjectFactory.h"
#include "Render/Resource/ShaderCompiler.h"

UShader* FShaderResourceCache::GetShader(const FString& FilePath) const
{
	const FString NormalizedFilePath = FPaths::Normalize(FilePath);
	auto It = Shaders.find(NormalizedFilePath);
	return (It != Shaders.end()) ? It->second : nullptr;
}

bool FShaderResourceCache::LoadShader(const FString& FilePath, const FString& VSEntryPoint, const FString& PSEntryPoint,
	const D3D_SHADER_MACRO* Defines, uint32 PermutationKey, ID3D11Device* Device)
{
	if (Device == nullptr)
	{
		return false;
	}

	const FString NormalizedFilePath = FPaths::Normalize(FilePath);
	UShader* Shader = nullptr;
	auto It = Shaders.find(NormalizedFilePath);

	if (It != Shaders.end())
	{
		Shader = It->second;
	}
	else
	{
		Shader = UObjectManager::Get().CreateObject<UShader>();
		Shader->FilePath = NormalizedFilePath;
		Shaders[NormalizedFilePath] = Shader;
	}

	FShader Permutation;

	TComPtr<ID3DBlob> VSBlob;
	TComPtr<ID3DBlob> PSBlob;

	TMap<FString, uint32> LocalTextureBindSlots;
	TMap<FString, FShaderVariableInfo> LocalVariableInfoMap;
	uint32 OutCBufferSize;

	FShaderCompileResult CompileResult = FShaderCompiler::CompileFromFile(NormalizedFilePath, VSEntryPoint, "vs_5_0", Defines, PermutationKey);
	if (CompileResult.bSuccess)
	{
		VSBlob = CompileResult.Blob;
		Shader->ReflectShader(VSBlob.Get(), Device, Permutation, LocalTextureBindSlots, LocalVariableInfoMap, OutCBufferSize);
	}
	else
	{
		return false;
	}

	CompileResult = FShaderCompiler::CompileFromFile(FilePath, PSEntryPoint, "ps_5_0", Defines, PermutationKey);
	if (CompileResult.bSuccess)
	{
		PSBlob = CompileResult.Blob;
		Shader->ReflectShader(PSBlob.Get(), Device, Permutation, LocalTextureBindSlots, LocalVariableInfoMap, OutCBufferSize);
	}
	else
	{
		return false;
	}

	HRESULT hr = Device->CreateVertexShader(VSBlob->GetBufferPointer(), VSBlob->GetBufferSize(), nullptr,
		&Permutation.VS);
	if (FAILED(hr))
	{
		UE_LOG_ERROR("Failed to create vertex shader: %s", NormalizedFilePath.c_str());
		return false;
	}

	hr = Device->CreatePixelShader(PSBlob->GetBufferPointer(), PSBlob->GetBufferSize(), nullptr,
		&Permutation.PS);
	if (FAILED(hr))
	{
		UE_LOG_ERROR("Failed to create pixel shader: %s", NormalizedFilePath.c_str());
		return false;
	}

	FShaderPermutationDesc Desc;
	Desc.VSEntryPoint = VSEntryPoint;
	Desc.PSEntryPoint = PSEntryPoint;
	Desc.PermutationKey = PermutationKey;
	for (const D3D_SHADER_MACRO* Define = Defines; Define && Define->Name; ++Define)
	{
		Desc.Defines.push_back({ Define->Name, Define->Definition });
	}

	Shader->SetPermutationDesc(PermutationKey, Desc);
	Shader->AddPermutation(PermutationKey, Permutation);

	return true;
}

bool FShaderResourceCache::EnsureShaderPermutation(const FString& FilePath, uint32 PermutationKey, ID3D11Device* Device)
{
	UShader* Shader = GetShader(FilePath);
	if (!Shader)
	{
		return false;
	}

	return Shader->EnsurePermutation(Device, PermutationKey);
}

void FShaderResourceCache::ReloadShader(const FString& FilePath, ID3D11Device* Device)
{
	UShader* Shader = GetShader(FilePath);
	if (Shader)
	{
		Shader->Reload(Device);
	}
}

FComputeShader* FShaderResourceCache::GetComputeShader(const FString& Key) const
{
	auto It = ComputeShaders.find(Key);
	if (It != ComputeShaders.end()) return It->second;
	// Fallback: try normalized path for backward compatibility
	const FString NormalizedFilePath = FPaths::Normalize(Key);
	It = ComputeShaders.find(NormalizedFilePath);
	return (It != ComputeShaders.end()) ? It->second : nullptr;
}

bool FShaderResourceCache::LoadComputeShader(const FString& FilePath, const FString& EntryPoint,
	const D3D_SHADER_MACRO* Defines, const FString& Key, ID3D11Device* Device)
{
	if (Device == nullptr)
	{
		return false;
	}

	const FString NormalizedFilePath = FPaths::Normalize(FilePath);
	const FString CacheKey = Key.empty() ? NormalizedFilePath : Key;

	FComputeShader* Shader = nullptr;
	auto It = ComputeShaders.find(CacheKey);
	if (It != ComputeShaders.end())
	{
		return true;
	}
	else
	{
		Shader = new FComputeShader();
		ComputeShaders[CacheKey] = Shader;
	}

	FShaderCompileResult CompileResult = FShaderCompiler::CompileFromFile(NormalizedFilePath, EntryPoint, "cs_5_0", Defines, 0);
	if (!CompileResult.bSuccess)
	{
		UE_LOG_ERROR("Compute Shader Compile Error (%s): %s", NormalizedFilePath.c_str(), CompileResult.ErrorMessage.c_str());
		return false;
	}

	TComPtr<ID3DBlob> CSBlob = CompileResult.Blob;

	HRESULT hr = Device->CreateComputeShader(CSBlob->GetBufferPointer(), CSBlob->GetBufferSize(), nullptr,
		&Shader->CS);
	if (FAILED(hr))
	{
		UE_LOG_ERROR("Failed to create compute shader: %s", NormalizedFilePath.c_str());
		return false;
	}

	return true;
}

void FShaderResourceCache::Release()
{
	for (auto& [Key, Shader] : Shaders)
	{
		if (Shader)
		{
			UObjectManager::Get().DestroyObject(Shader);
		}
	}
	Shaders.clear();

	for (auto& [Key, Shader] : ComputeShaders)
	{
		if (Shader)
		{
			Shader->Release();
			delete Shader;
		}
	}
	ComputeShaders.clear();
}
