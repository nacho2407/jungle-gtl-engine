#include "ShaderManager.h"
#include "ShaderMap.h"
#include "Shader.h"

FShaderManager::~FShaderManager()
{
	Release();
}

bool FShaderManager::LoadVertexShader(ID3D11Device* Device, const wchar_t* FilePath)
{
	VS = FShaderMap::Get().GetOrCreateVertexShader(Device, FilePath);
	return VS != nullptr;
}

bool FShaderManager::LoadPixelShader(ID3D11Device* Device, const wchar_t* FilePath)
{
	PS = FShaderMap::Get().GetOrCreatePixelShader(Device, FilePath);
	return PS != nullptr;
}

void FShaderManager::Bind(ID3D11DeviceContext* DeviceContext)
{
	if (VS) VS->Bind(DeviceContext);
	if (PS) PS->Bind(DeviceContext);
}

void FShaderManager::Release()
{
	VS.reset();
	PS.reset();
}
