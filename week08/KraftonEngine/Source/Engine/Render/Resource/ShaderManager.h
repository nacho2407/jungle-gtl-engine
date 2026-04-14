#pragma once

#include "Core/Singleton.h"
#include "Render/Resource/Shader.h"
#include "Core/CoreTypes.h"
#include <memory>
enum class EShaderType : uint32
{
	Default = 0,
	Primitive,
	Gizmo,
	Editor,
	StaticMesh,
	Decal,
	OutlinePostProcess,
	Font,
	OverlayFont,
	SubUV,
	Billboard,
	HeightFog,
	SceneDepth,
	MAX,
};

class FShaderManager : public TSingleton<FShaderManager>
{
	friend class TSingleton<FShaderManager>;

public:
	void Initialize(ID3D11Device* InDevice);
	void Release();

	FShader* GetShader(EShaderType InType);
	FShader* GetCustomShader(const FString& Key);

	FShader* CreateCustomShader(ID3D11Device* InDevice, const wchar_t* InFilePath,const D3D11_INPUT_ELEMENT_DESC* InInputElements = nullptr,uint32 InInputElementCount = 0);

private:
	FShaderManager() = default;

	FShader Shaders[(uint32)EShaderType::MAX];
	TMap<FString, std::unique_ptr< FShader>> CustomShaderCache; // 커스텀 셰이더 캐시 (경로 → 셰이더)

	bool bIsInitialized = false;
};
