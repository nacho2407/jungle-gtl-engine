#include "ShaderCompiler.h"
#include "Core/Paths.h"

#include <d3dcompiler.h>

FShaderCompileResult FShaderCompiler::CompileFromFile(const FString& FilePath, const FString& EntryPoint, const FString& Target,
	const D3D_SHADER_MACRO* Defines, uint32 PermutationKey)
{
	FShaderCompileResult Result;
	ID3DBlob* ErrorBlob = nullptr;
	HRESULT hr = D3DCompileFromFile(FPaths::ToWide(FilePath).c_str(), Defines, D3D_COMPILE_STANDARD_FILE_INCLUDE, EntryPoint.c_str(), Target.c_str(), 0, 0, &Result.Blob, &ErrorBlob);
	
	if (FAILED(hr))
	{
		Result.bSuccess = false;
		if (ErrorBlob)
		{
			Result.ErrorMessage = FString((char*)ErrorBlob->GetBufferPointer());
			ErrorBlob->Release();
		}
		else
		{
			Result.ErrorMessage = "Unknown error during shader compilation: " + FilePath;
		}
	}
	else
	{
		Result.bSuccess = true;
	}

	return Result;
}