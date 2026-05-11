#pragma once

#include "Core/CoreTypes.h"
#include "Render/Resource/ComputeShader.h"
#include "Render/Resource/ShaderTypes.h"

#include <d3d11.h>

class FShaderResourceCache
{
public:
	// Shader Stage는 VS / PS 단위로 따로 캐싱합니다.
	// 기존 UShader처럼 VS+PS를 한 덩어리로 묶지 않기 때문에 PS 재사용이 가능합니다.
	FVertexShader* GetOrCreateVertexShader(const FShaderStageKey& Key, const D3D_SHADER_MACRO* Defines, ID3D11Device* Device);
	FPixelShader* GetOrCreatePixelShader(const FShaderStageKey& Key, const D3D_SHADER_MACRO* Defines, ID3D11Device* Device);

	// 실제 Draw에서 바인딩할 Program입니다.
	// 내부적으로는 필요한 VS/PS Stage를 먼저 가져오고, 조합만 ProgramCache에 저장합니다.
	FShaderProgram* GetOrCreateProgram(
		const FShaderStageKey& VSKey,
		const FShaderStageKey& PSKey,
		const D3D_SHADER_MACRO* VSDefines,
		const D3D_SHADER_MACRO* PSDefines,
		ID3D11Device* Device);

	FComputeShader* GetComputeShader(const FString& Key) const;
	bool LoadComputeShader(const FString& FilePath, const FString& EntryPoint,
		const D3D_SHADER_MACRO* Defines, const FString& Key, ID3D11Device* Device);

	// Shader Hot Reload용 무효화 함수입니다.
	// 파일이 바뀌면 해당 파일을 참조하는 Stage/Program만 지우고, 다음 Draw에서 다시 컴파일합니다.
	void InvalidateShaderFile(const FString& FilePath);

	void Release();

private:
	// Compute는 VS/PS Program 조합과 별개라 기존처럼 독립 캐시로 관리합니다.
	TMap<FString, FComputeShader*> ComputeShaders;

	// Stage Cache: 파일 + EntryPoint + Target + PermutationKey 단위.
	TMap<FShaderStageKey, FVertexShader*, FShaderStageKeyHasher> VertexShaders;
	TMap<FShaderStageKey, FPixelShader*, FShaderStageKeyHasher> PixelShaders;

	// Program Cache: 이미 컴파일된 VS/PS Stage의 조합만 저장합니다.
	TMap<FShaderProgramKey, FShaderProgram*, FShaderProgramKeyHasher> ShaderPrograms;
};
