#pragma once

#include "Core/Singleton.h"
#include "Render/Resource/Shader.h"
#include "Core/CoreTypes.h"
#include <memory>
#include <functional>
#include <string_view>

struct FShaderKey
{
	FString Path;
	uint64  PathHash = 0;
	uint64  DefinesHash = 0;

	FShaderKey(const FString& InPath)
		: Path(InPath)
		, PathHash(std::hash<FString>{}(InPath))
		, DefinesHash(0)
	{}

	FShaderKey(const FString& InPath, const D3D_SHADER_MACRO* InDefines)
		: Path(InPath)
		, PathHash(std::hash<FString>{}(InPath))
		, DefinesHash(HashDefines(InDefines))
	{}

	bool operator==(const FShaderKey& Other) const
	{
		return PathHash == Other.PathHash
			&& DefinesHash == Other.DefinesHash;
	}

private:
	static uint64 HashDefines(const D3D_SHADER_MACRO* Defines)
	{
		if (!Defines)
		{
			return 0;
		}

		uint64 H = 0;
		for (const D3D_SHADER_MACRO* D = Defines; D->Name != nullptr; ++D)
		{
			uint64 NameHash = std::hash<std::string_view>{}(D->Name);
			uint64 ValHash = D->Definition ? std::hash<std::string_view>{}(D->Definition) : 0;
			H ^= NameHash * 0x9e3779b97f4a7c15ULL + ValHash;
		}
		return H;
	}
};

namespace std
{
	template<> struct hash<FShaderKey>
	{
		size_t operator()(const FShaderKey& K) const
		{
			return static_cast<size_t>(K.PathHash ^ (K.DefinesHash * 0x9e3779b97f4a7c15ULL));
		}
	};
}

namespace EShaderPath
{
	inline constexpr const char* Primitive = "Shaders/Geometry/Primitive.hlsl";
	inline constexpr const char* UberLit = "Shaders/Geometry/UberLit.hlsl";
	inline constexpr const char* Decal = "Shaders/Geometry/Decal.hlsl";
	inline constexpr const char* FakeLight = "Shaders/Geometry/FakeLight.hlsl";

	inline constexpr const char* Editor = "Shaders/Editor/Editor.hlsl";
	inline constexpr const char* Gizmo = "Shaders/Editor/Gizmo.hlsl";

	inline constexpr const char* FXAA = "Shaders/PostProcess/FXAA.hlsl";
	inline constexpr const char* Outline = "Shaders/PostProcess/Outline.hlsl";
	inline constexpr const char* SceneDepth = "Shaders/PostProcess/SceneDepth.hlsl";
	inline constexpr const char* SceneNormal = "Shaders/PostProcess/SceneNormal.hlsl";
	inline constexpr const char* HeightFog = "Shaders/PostProcess/HeightFog.hlsl";
	inline constexpr const char* LightCulling = "Shaders/PostProcess/LightCulling.hlsl";

	inline constexpr const char* Font = "Shaders/UI/Font.hlsl";
	inline constexpr const char* OverlayFont = "Shaders/UI/OverlayFont.hlsl";
	inline constexpr const char* SubUV = "Shaders/UI/SubUV.hlsl";
	inline constexpr const char* Billboard = "Shaders/UI/Billboard.hlsl";
}

namespace EUberLitDefines
{
	inline const D3D_SHADER_MACRO Default[] = { {"LIGHTING_MODEL_PHONG", "1"}, {nullptr, nullptr} };
	inline const D3D_SHADER_MACRO Unlit[] = { {"LIGHTING_MODEL_UNLIT", "1"}, {nullptr, nullptr} };
	inline const D3D_SHADER_MACRO Gouraud[] = { {"LIGHTING_MODEL_GOURAUD", "1"}, {nullptr, nullptr} };
	inline const D3D_SHADER_MACRO Lambert[] = { {"LIGHTING_MODEL_LAMBERT", "1"}, {nullptr, nullptr} };
	inline const D3D_SHADER_MACRO Phong[] = { {"LIGHTING_MODEL_PHONG", "1"}, {nullptr, nullptr} };
	inline const D3D_SHADER_MACRO Toon[] = { {"LIGHTING_MODEL_TOON", "1"}, {nullptr, nullptr} };
}

class FShaderManager : public TSingleton<FShaderManager>
{
	friend class TSingleton<FShaderManager>;

public:
	void Initialize(ID3D11Device* InDevice);
	void Release();

	FShader* GetOrCreate(const FShaderKey& Key);
	FShader* PreCompile(const FShaderKey& Key, const D3D_SHADER_MACRO* Defines);
	FShader* GetOrCreate(const FString& Path) { return GetOrCreate(FShaderKey(Path)); }
	FShader* FindOrCreate(const FString& Path);

private:
	FShaderManager() = default;

	ID3D11Device* CachedDevice = nullptr;
	TMap<FShaderKey, std::unique_ptr<FShader>> ShaderCache;
	bool bIsInitialized = false;
};
