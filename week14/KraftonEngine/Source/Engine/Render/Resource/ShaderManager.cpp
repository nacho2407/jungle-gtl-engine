#include "ShaderManager.h"
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

	Shaders[(uint32)EShaderType::Primitive].Create(InDevice, L"Shaders/Primitive.hlsl", "VS", "PS");
	Shaders[(uint32)EShaderType::Gizmo].Create(InDevice, L"Shaders/Gizmo.hlsl", "VS", "PS");
	Shaders[(uint32)EShaderType::Editor].Create(InDevice, L"Shaders/Editor.hlsl", "VS", "PS");
	// [UberLit] StaticMesh → UberLit 교체 (DEBUG_LIGHTS=1, 기본 Blinn-Phong)
	// 팀원 A ViewMode 시스템 완성 시 EShaderType::UberLit_xxx 로 분리 예정
	Shaders[(uint32)EShaderType::StaticMesh].Create(InDevice, L"Shaders/UberLit.hlsl", "VS", "PS");
	Shaders[(uint32)EShaderType::Decal].Create(InDevice, L"Shaders/DecalShader.hlsl", "VS", "PS");
	Shaders[(uint32)EShaderType::OutlinePostProcess].Create(InDevice, L"Shaders/OutlinePostProcess.hlsl", "VS", "PS");
	Shaders[(uint32)EShaderType::SceneDepth].Create(InDevice, L"Shaders/SceneDepth.hlsl", "VS", "PS");
	Shaders[(uint32)EShaderType::FXAA].Create(InDevice, L"Shaders/FXAA.hlsl", "VS", "PS");
	Shaders[(uint32)EShaderType::Font].Create(InDevice, L"Shaders/ShaderFont.hlsl", "VS", "PS");
	Shaders[(uint32)EShaderType::OverlayFont].Create(InDevice, L"Shaders/ShaderOverlayFont.hlsl", "VS", "PS");
	Shaders[(uint32)EShaderType::SubUV].Create(InDevice, L"Shaders/ShaderSubUV.hlsl", "VS", "PS");
	Shaders[(uint32)EShaderType::Billboard].Create(InDevice, L"Shaders/ShaderBillboard.hlsl", "VS", "PS");
	Shaders[(uint32)EShaderType::HeightFog].Create(InDevice, L"Shaders/HeightFog.hlsl", "VS", "PS");

	bIsInitialized = true;
}



void FShaderManager::Release()
{
	for (uint32 i = 0; i < (uint32)EShaderType::MAX; ++i)
	{
		Shaders[i].Release();
	}

	for (auto& [Key, Shader] : CustomShaderCache)
		Shader->Release();

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
		return It->second.get();  // 이미 캐시에 있으면 반환
	}
	return nullptr;
}

FShader* FShaderManager::CreateCustomShader(ID3D11Device* InDevice, const wchar_t* InFilePath)
{
	FString Key = WStringToString(InFilePath);

	auto It = CustomShaderCache.find(Key);
	if (It != CustomShaderCache.end())
	{
		return It->second.get();  // 이미 캐시에 있으면 반환
	}

	auto NewShader = std::make_unique<FShader>();
	NewShader->Create(InDevice, InFilePath, "VS", "PS");
	auto* RawPtr = NewShader.get();
	CustomShaderCache[Key] = std::move(NewShader);
	return RawPtr;
}



