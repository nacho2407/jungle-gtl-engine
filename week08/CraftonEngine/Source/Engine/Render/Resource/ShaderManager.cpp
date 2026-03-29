#include "ShaderManager.h"
#include "Render/Types/VertexTypes.h"

void FShaderManager::Initialize(ID3D11Device* InDevice)
{
	if (bIsInitialized) return;

	Shaders[(uint32)EShaderType::Primitive].Create(InDevice, L"Shaders/Primitive.hlsl",
		"VS", "PS", FVertexInputLayout, ARRAYSIZE(FVertexInputLayout));

	Shaders[(uint32)EShaderType::Gizmo].Create(InDevice, L"Shaders/Gizmo.hlsl",
		"VS", "PS", FVertexInputLayout, ARRAYSIZE(FVertexInputLayout));

	Shaders[(uint32)EShaderType::Editor].Create(InDevice, L"Shaders/Editor.hlsl",
		"VS", "PS", FVertexInputLayout, ARRAYSIZE(FVertexInputLayout));

	Shaders[(uint32)EShaderType::Outline].Create(InDevice, L"Shaders/Outline.hlsl",
		"VS", "PS", FVertexInputLayout, ARRAYSIZE(FVertexInputLayout));

	Shaders[(uint32)EShaderType::OutlinePNCT].Create(InDevice, L"Shaders/OutlinePNCT.hlsl",
		"VS", "PS", FVertexPNCTInputLayout, ARRAYSIZE(FVertexPNCTInputLayout));

	Shaders[(uint32)EShaderType::StaticMesh].Create(InDevice, L"Shaders/StaticMeshShader.hlsl",
		"VS", "PS", FVertexPNCTInputLayout, ARRAYSIZE(FVertexPNCTInputLayout));

	bIsInitialized = true;
}

void FShaderManager::Release()
{
	for (uint32 i = 0; i < (uint32)EShaderType::MAX; ++i)
	{
		Shaders[i].Release();
	}
	bIsInitialized = false;
}

FShader* FShaderManager::GetShader(EShaderType InType)
{
	uint32 Idx = (uint32)InType;
	if (Idx < (uint32)EShaderType::MAX)
	{
		return &Shaders[Idx];
	}
	return nullptr;
}
