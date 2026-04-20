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
	// Geometry — 메시 렌더링
	inline constexpr const char* Primitive         = "Shaders/Geometry/Primitive.hlsl";
	inline constexpr const char* UberLit           = "Shaders/Geometry/UberLit.hlsl";
	inline constexpr const char* Decal             = "Shaders/Geometry/Decal.hlsl";
	inline constexpr const char* FakeLight         = "Shaders/Geometry/FakeLight.hlsl";

	// Editor — 에디터 전용
	inline constexpr const char* Editor            = "Shaders/Editor/Editor.hlsl";
	inline constexpr const char* Gizmo             = "Shaders/Editor/Gizmo.hlsl";

	// PostProcess — 후처리
	inline constexpr const char* FXAA              = "Shaders/PostProcess/FXAA.hlsl";
	inline constexpr const char* Outline           = "Shaders/PostProcess/Outline.hlsl";
	inline constexpr const char* SceneDepth        = "Shaders/PostProcess/SceneDepth.hlsl";
	inline constexpr const char* SceneNormal       = "Shaders/PostProcess/SceneNormal.hlsl";
	inline constexpr const char* HeightFog         = "Shaders/PostProcess/HeightFog.hlsl";

	// UI — 2D/텍스트/파티클
	inline constexpr const char* Font              = "Shaders/UI/Font.hlsl";
	inline constexpr const char* OverlayFont       = "Shaders/UI/OverlayFont.hlsl";
	inline constexpr const char* SubUV             = "Shaders/UI/SubUV.hlsl";
	inline constexpr const char* Billboard         = "Shaders/UI/Billboard.hlsl";
}

// ============================================================
// UberLit permutation 매크로 (Initialize + SelectEffectiveShader에서 공유)
// ============================================================
namespace EUberLitDefines
{
	inline const D3D_SHADER_MACRO Unlit[]    = { {"LIGHTING_MODEL_UNLIT","1"}, {nullptr,nullptr} };
	inline const D3D_SHADER_MACRO Gouraud[]  = { {"LIGHTING_MODEL_GOURAUD","1"}, {"USE_TILE_CULLING","0"}, {nullptr,nullptr} };
	inline const D3D_SHADER_MACRO Lambert[]  = { {"LIGHTING_MODEL_LAMBERT","1"}, {"USE_TILE_CULLING","1"}, {nullptr,nullptr} };
	inline const D3D_SHADER_MACRO Phong[]    = { {"LIGHTING_MODEL_PHONG","1"},   {"USE_TILE_CULLING","1"}, {nullptr,nullptr} };
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

	// MaterialManager 통합 — 경로로 셰이더 조회, 없으면 컴파일
	FShader* FindOrCreate(const FString& Path);

private:
	FShaderManager() = default;

	ID3D11Device* CachedDevice = nullptr;
	TMap<FShaderKey, std::unique_ptr<FShader>> ShaderCache;

	bool bIsInitialized = false;
};
