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

	Shaders[(uint32)EShaderType::StaticMesh].Create(InDevice, L"Shaders/StaticMeshShader.hlsl",
		"VS", "PS", FVertexPNCTInputLayout, ARRAYSIZE(FVertexPNCTInputLayout));

	// PostProcess outline: fullscreen quad (InputLayout 없음)
	Shaders[(uint32)EShaderType::OutlinePostProcess].Create(InDevice, L"Shaders/OutlinePostProcess.hlsl",
		"VS", "PS", nullptr, 0);

	Shaders[(uint32)EShaderType::FXAA].Create(InDevice, L"Shaders/FXAA.hlsl",
		"VS", "PS", nullptr, 0);

	// Batcher 셰이더 (FTextureVertex: POSITION + TEXCOORD)
	Shaders[(uint32)EShaderType::Font].Create(InDevice, L"Shaders/ShaderFont.hlsl",
		"VS", "PS", FTextureVertexInputLayout, ARRAYSIZE(FTextureVertexInputLayout));

	Shaders[(uint32)EShaderType::OverlayFont].Create(InDevice, L"Shaders/ShaderOverlayFont.hlsl",
		"VS", "PS", FTextureVertexInputLayout, ARRAYSIZE(FTextureVertexInputLayout));

	Shaders[(uint32)EShaderType::SubUV].Create(InDevice, L"Shaders/ShaderSubUV.hlsl",
		"VS", "PS", FTextureVertexInputLayout, ARRAYSIZE(FTextureVertexInputLayout));

	Shaders[(uint32)EShaderType::Billboard].Create(InDevice, L"Shaders/ShaderBillboard.hlsl",
		"VS", "PS", FTextureVertexInputLayout, ARRAYSIZE(FTextureVertexInputLayout));

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
