#pragma once

#include "Core/Singleton.h"
#include "Render/Resource/Shader.h"
#include "Core/CoreTypes.h"

enum class EShaderType : uint32
{
	Primitive,
	Gizmo,
	Editor,
	StaticMesh,
	OutlinePostProcess,
	Font,
	OverlayFont,
	SubUV,
	Billboard,
	MAX,
};

class FShaderManager : public TSingleton<FShaderManager>
{
	friend class TSingleton<FShaderManager>;

public:
	void Initialize(ID3D11Device* InDevice);
	void Release();

	FShader* GetShader(EShaderType InType);

private:
	FShaderManager() = default;

	FShader Shaders[(uint32)EShaderType::MAX];
	bool bIsInitialized = false;
};
