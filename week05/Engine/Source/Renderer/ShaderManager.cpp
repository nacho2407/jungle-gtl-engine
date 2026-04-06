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

bool FShaderManager::LoadComputeShader(ID3D11Device* Device, const wchar_t* FilePath, const char* EntryPoint)
{
	CS = FShaderMap::Get().GetOrCreateComputeShader(Device, FilePath, EntryPoint);
	return CS != nullptr;
}

void FShaderManager::Bind(ID3D11DeviceContext* DeviceContext)
{
	if (VS) VS->Bind(DeviceContext);
	if (PS) PS->Bind(DeviceContext);
}

void FShaderManager::BindCompute(ID3D11DeviceContext* DeviceContext)
{
	if (CS) CS->Bind(DeviceContext);
}

void FShaderManager::DispatchCompute(ID3D11DeviceContext* DeviceContext, uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ)
{
	if (CS)
	{
		CS->Dispatch(DeviceContext, ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
	}
}

void FShaderManager::Release()
{
	VS.reset();
	PS.reset();
	CS.reset();
}
