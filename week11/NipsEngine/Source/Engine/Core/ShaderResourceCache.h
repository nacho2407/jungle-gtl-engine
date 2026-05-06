#pragma once

#include "Core/CoreTypes.h"
#include "Render/Resource/ComputeShader.h"
#include "Render/Resource/Shader.h"

#include <d3d11.h>

class FShaderResourceCache
{
public:
	UShader* GetShader(const FString& FilePath) const;
	bool LoadShader(const FString& FilePath, const FString& VSEntryPoint, const FString& PSEntryPoint,
		const D3D_SHADER_MACRO* Defines, uint32 PermutationKey, ID3D11Device* Device);
	bool EnsureShaderPermutation(const FString& FilePath, uint32 PermutationKey, ID3D11Device* Device);
	void ReloadShader(const FString& FilePath, ID3D11Device* Device);

	FComputeShader* GetComputeShader(const FString& Key) const;
	bool LoadComputeShader(const FString& FilePath, const FString& EntryPoint,
		const D3D_SHADER_MACRO* Defines, const FString& Key, ID3D11Device* Device);

	void Release();

private:
	TMap<FString, UShader*> Shaders;
	TMap<FString, FComputeShader*> ComputeShaders;
};
