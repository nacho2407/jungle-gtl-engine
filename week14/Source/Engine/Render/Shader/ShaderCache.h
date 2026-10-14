#pragma once

#include "Render/Types/RenderTypes.h"

namespace ShaderCache
{
	std::string BuildShaderCacheKey(const wchar_t* FilePath, const char* EntryPoint, const char* Target, const D3D_SHADER_MACRO* Defines);
	bool LoadShaderBlob(const std::string& CacheKey, ID3DBlob** OutBlob);
	void SaveShaderBlob(const std::string& CacheKey, ID3DBlob* Blob);
}
