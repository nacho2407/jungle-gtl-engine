#pragma once

#include "Core/Singleton.h"
#include "Render/Resource/Shader.h"
#include "Core/CoreTypes.h"
#include <memory>
#include <functional>

// ============================================================
// FShaderKey — 셰이더 캐시 조회 키 (경로 + 매크로 조합)
// ============================================================
struct FShaderKey
{
	FString Path;
	uint64  PathHash    = 0;
	uint64  DefinesHash = 0;

	// Defines 없는 단순 셰이더
	FShaderKey(const FString& InPath)
		: Path(InPath)
		, PathHash(std::hash<FString>{}(InPath))
		, DefinesHash(0)
	{}

	// Defines 포함 (D3D_SHADER_MACRO null-terminated 배열)
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
		if (!Defines) return 0;
		uint64 H = 0;
		for (const D3D_SHADER_MACRO* D = Defines; D->Name != nullptr; ++D)
		{
			uint64 NameHash = std::hash<std::string_view>{}(D->Name);
			uint64 ValHash  = D->Definition ? std::hash<std::string_view>{}(D->Definition) : 0;
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

// ============================================================
// 시스템 셰이더 경로 상수
// ============================================================
namespace EShaderPath
{
	inline constexpr const char* Primitive         = "Shaders/Primitive.hlsl";
	inline constexpr const char* Gizmo             = "Shaders/Gizmo.hlsl";
	inline constexpr const char* Editor            = "Shaders/Editor.hlsl";
	inline constexpr const char* UberLit           = "Shaders/UberLit.hlsl";
	inline constexpr const char* Decal             = "Shaders/DecalShader.hlsl";
	inline constexpr const char* OutlinePostProcess = "Shaders/OutlinePostProcess.hlsl";
	inline constexpr const char* Font              = "Shaders/ShaderFont.hlsl";
	inline constexpr const char* OverlayFont       = "Shaders/ShaderOverlayFont.hlsl";
	inline constexpr const char* SubUV             = "Shaders/ShaderSubUV.hlsl";
	inline constexpr const char* Billboard         = "Shaders/ShaderBillboard.hlsl";
	inline constexpr const char* HeightFog         = "Shaders/HeightFog.hlsl";
	inline constexpr const char* SceneDepth        = "Shaders/SceneDepth.hlsl";
	inline constexpr const char* SceneNormal       = "Shaders/SceneNormal.hlsl";
	inline constexpr const char* FXAA              = "Shaders/FXAA.hlsl";
}

// ============================================================
// UberLit permutation 매크로 (Initialize + SelectEffectiveShader에서 공유)
// ============================================================
namespace EUberLitDefines
{
	inline const D3D_SHADER_MACRO Default[]  = { {"DEBUG_LIGHTS","0"}, {nullptr,nullptr} };
	inline const D3D_SHADER_MACRO Gouraud[]  = { {"LIGHTING_MODEL_GOURAUD","1"}, {"DEBUG_LIGHTS","0"}, {"USE_TILE_CULLING","0"}, {nullptr,nullptr} };
	inline const D3D_SHADER_MACRO Lambert[]  = { {"LIGHTING_MODEL_LAMBERT","1"}, {"DEBUG_LIGHTS","0"}, {"USE_TILE_CULLING","1"}, {nullptr,nullptr} };
	inline const D3D_SHADER_MACRO Phong[]    = { {"LIGHTING_MODEL_PHONG","1"},   {"DEBUG_LIGHTS","0"}, {"USE_TILE_CULLING","1"}, {nullptr,nullptr} };
}

// ============================================================
// FShaderManager — 경로+매크로 기반 단일 캐시 셰이더 관리
// ============================================================
class FShaderManager : public TSingleton<FShaderManager>
{
	friend class TSingleton<FShaderManager>;

public:
	void Initialize(ID3D11Device* InDevice);
	void Release();

	// 기본 API — 캐시 히트 시 반환, 미스 시 컴파일 후 캐싱
	FShader* GetOrCreate(const FShaderKey& Key);

	// 매크로 포함 사전 컴파일 — Initialize에서만 호출
	FShader* PreCompile(const FShaderKey& Key, const D3D_SHADER_MACRO* Defines);

	// 경로 전용 편의 오버로드 (Defines 없음)
	FShader* GetOrCreate(const FString& Path) { return GetOrCreate(FShaderKey(Path)); }

	// MaterialManager 통합 — 경로로 기 등록 셰이더 우선 조회, 없으면 컴파일
	FShader* FindOrCreate(const FString& Path);

	// 경로 별칭 등록 — 레거시 경로를 기존 셰이더로 매핑
	void RegisterAlias(const FString& AliasPath, const FShaderKey& TargetKey);

private:
	FShaderManager() = default;

	ID3D11Device* CachedDevice = nullptr;
	TMap<FShaderKey, std::unique_ptr<FShader>> ShaderCache;
	TMap<FString, FShader*> AliasMap;	// 레거시 경로 → 기존 셰이더

	bool bIsInitialized = false;
};
