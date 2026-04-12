#include "ShaderManager.h"
#include "Render/Types/VertexTypes.h"
#include <string>
#include <Windows.h>

namespace
{
	inline std::string WStringToString(const std::wstring& wstr)
	{
		if (wstr.empty()) return std::string();
	
		int sizeNeeded = WideCharToMultiByte(
			CP_UTF8,
			0,
			wstr.data(),
			(int)wstr.size(),
			nullptr,
			0,
			nullptr,
			nullptr
		);
	
		if (sizeNeeded <= 0) return std::string();
	
		std::string result(sizeNeeded, 0);
	
		WideCharToMultiByte(
			CP_UTF8,
			0,
			wstr.data(),
			(int)wstr.size(),
			&result[0],
			sizeNeeded,
			nullptr,
			nullptr
		);
	
		return result;
	}
}

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

	for (auto& [Key, Shader] : CustomShaderCache)
		Shader.Release();

	CustomShaderCache.clear();

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

FShader* FShaderManager::GetCustomShader(const FString& Key)
{
	auto It = CustomShaderCache.find(Key);
	if (It != CustomShaderCache.end())
	{
		return &It->second;  // 이미 캐시에 있으면 반환
	}
	return nullptr;
}

FShader* FShaderManager::CreateCustomShader(ID3D11Device* InDevice, const wchar_t* InFilePath, const D3D11_INPUT_ELEMENT_DESC* InInputElements, uint32 InInputElementCount)
{
	FString Key = WStringToString(InFilePath);

	auto It = CustomShaderCache.find(Key);
	if (It != CustomShaderCache.end())
	{
		return &It->second;  // 이미 캐시에 있으면 반환
	}

	FShader NewShader;
	NewShader.Create(InDevice, InFilePath, "VS", "PS", InInputElements, InInputElementCount);

	CustomShaderCache[Key] = std::move(NewShader);
	return &CustomShaderCache[Key];
}



