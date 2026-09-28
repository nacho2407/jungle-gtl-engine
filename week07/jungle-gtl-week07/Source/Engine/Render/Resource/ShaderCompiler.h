#pragma once

#include "Core/CoreTypes.h"
#include "Core/Containers/String.h"

#include <d3d11.h>

struct FShaderCompileResult
{
	bool bSuccess = false;
	FString ErrorMessage;
	ID3DBlob* Blob = nullptr;
};

class FShaderCompiler
{
public:
	static FShaderCompileResult CompileFromFile(const FString& FilePath, const FString& EntryPoint, const FString& Target,
		const D3D_SHADER_MACRO* Defines, uint32 PermutationKey);
};