#pragma once

#include "Render/Types/RenderTypes.h"
#include "Platform/Paths.h"
#include <fstream>

// ============================================================
// FShaderInclude — Shaders/ 디렉토리를 인클루드 루트로 사용하는 커스텀 핸들러
//
// OutIncludes가 설정되어 있으면 Open() 시 include 파일 경로를 수집한다.
// 경로 형식: "Common/ConstantBuffers.hlsl" (Shaders/ 기준 상대 경로)
// ============================================================
class FShaderInclude : public ID3DInclude
{
public:
	// 의존성 수집이 필요할 때 외부에서 설정
	TArray<FString>* OutIncludes = nullptr;

	HRESULT __stdcall Open(
		D3D_INCLUDE_TYPE IncludeType,
		LPCSTR pFileName,
		LPCVOID pParentData,
		LPCVOID* ppData,
		UINT* pBytes) override
	{
		std::wstring FullPath = FPaths::ShaderDir() + FPaths::ToWide(pFileName);

		std::ifstream File(FullPath, std::ios::binary | std::ios::ate);
		if (!File.is_open()) return E_FAIL;

		auto Size = File.tellg();
		File.seekg(0, std::ios::beg);

		char* Buffer = new char[static_cast<size_t>(Size)];
		File.read(Buffer, Size);

		*ppData = Buffer;
		*pBytes = static_cast<UINT>(Size);

		// include 경로 수집 (슬래시 정규화)
		if (OutIncludes)
		{
			FString IncludePath = pFileName;
			for (auto& Ch : IncludePath)
			{
				if (Ch == '\\') Ch = '/';
			}
			OutIncludes->push_back(IncludePath);
		}

		return S_OK;
	}

	HRESULT __stdcall Close(LPCVOID pData) override
	{
		delete[] static_cast<const char*>(pData);
		return S_OK;
	}
};
