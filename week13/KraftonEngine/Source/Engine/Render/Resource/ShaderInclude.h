#pragma once

#include "Render/Types/RenderTypes.h"
#include "Platform/Paths.h"
#include <fstream>

// ============================================================
// FShaderInclude — Shaders/ 디렉토리를 인클루드 루트로 사용하는 커스텀 핸들러
// ============================================================
class FShaderInclude : public ID3DInclude
{
public:
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
		return S_OK;
	}

	HRESULT __stdcall Close(LPCVOID pData) override
	{
		delete[] static_cast<const char*>(pData);
		return S_OK;
	}
};
