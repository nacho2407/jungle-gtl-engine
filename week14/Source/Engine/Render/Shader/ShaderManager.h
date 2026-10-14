#pragma once

#include "Core/Singleton.h"
#include "Render/Shader/Shader.h"
#include "Core/Types/CoreTypes.h"
#include "Engine/Platform/DirectoryWatcher.h"
#include <memory>
#include <string_view>

struct FShaderKey
{
	FString Path;
	FString VSEntryPoint = "VS";
	FString PSEntryPoint = "PS";
	uint64  PathHash = 0;
	uint64  DefinesHash = 0;
	uint64  EntryHash = 0;

	FShaderKey(const FString& InPath)
		: Path(InPath)
		, PathHash(std::hash<FString>{}(InPath))
		, DefinesHash(0)
		, EntryHash(HashEntryPoints(VSEntryPoint, PSEntryPoint))
	{}

	FShaderKey(const FString& InPath, const D3D_SHADER_MACRO* InDefines)
		: Path(InPath)
		, PathHash(std::hash<FString>{}(InPath))
		, DefinesHash(HashDefines(InDefines))
		, EntryHash(HashEntryPoints(VSEntryPoint, PSEntryPoint))
	{}

	FShaderKey(const FString& InPath, const D3D_SHADER_MACRO* InDefines, const FString& InVSEntryPoint, const FString& InPSEntryPoint = "PS")
		: Path(InPath)
		, VSEntryPoint(InVSEntryPoint)
		, PSEntryPoint(InPSEntryPoint)
		, PathHash(std::hash<FString>{}(InPath))
		, DefinesHash(HashDefines(InDefines))
		, EntryHash(HashEntryPoints(VSEntryPoint, PSEntryPoint))
	{}

	bool operator==(const FShaderKey& Other) const
	{
		return PathHash == Other.PathHash
			&& DefinesHash == Other.DefinesHash
			&& EntryHash == Other.EntryHash;
	}

private:
	static uint64 HashEntryPoints(const FString& VSEntryPoint, const FString& PSEntryPoint)
	{
		const uint64 VSHash = std::hash<FString>{}(VSEntryPoint);
		const uint64 PSHash = std::hash<FString>{}(PSEntryPoint);
		return VSHash ^ (PSHash * 0x9e3779b97f4a7c15ULL);
	}

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
			return static_cast<size_t>(K.PathHash
				^ (K.DefinesHash * 0x9e3779b97f4a7c15ULL)
				^ (K.EntryHash * 0xbf58476d1ce4e5b9ULL));
		}
	};
}

namespace EShaderPath
{
	inline constexpr const char* Primitive = "Shaders/Geometry/Primitive.hlsl";
	inline constexpr const char* UberLit = "Shaders/Geometry/UberLit.hlsl";
	inline constexpr const char* Decal = "Shaders/Geometry/Decal.hlsl";
	inline constexpr const char* Particle = "Shaders/Geometry/Particle.hlsl";
	inline constexpr const char* PreDepth = "Shaders/Geometry/PreDepth.hlsl";

	inline constexpr const char* Editor = "Shaders/Editor/Editor.hlsl";
	inline constexpr const char* Gizmo = "Shaders/Editor/Gizmo.hlsl";

	inline constexpr const char* FXAA = "Shaders/PostProcess/FXAA.hlsl";
	inline constexpr const char* Outline = "Shaders/PostProcess/Outline.hlsl";
	inline constexpr const char* SceneDepth = "Shaders/PostProcess/SceneDepth.hlsl";
	inline constexpr const char* SceneNormal = "Shaders/PostProcess/SceneNormal.hlsl";
	inline constexpr const char* HeightFog = "Shaders/PostProcess/HeightFog.hlsl";
	inline constexpr const char* LightCulling = "Shaders/PostProcess/LightCulling.hlsl";
	inline constexpr const char* GammaCorrection = "Shaders/PostProcess/GammaCorrection.hlsl";

	inline constexpr const char* Font = "Shaders/UI/Font.hlsl";
	inline constexpr const char* OverlayFont = "Shaders/UI/OverlayFont.hlsl";
	inline constexpr const char* SubUV = "Shaders/UI/SubUV.hlsl";
	inline constexpr const char* Billboard = "Shaders/UI/Billboard.hlsl";

	inline constexpr const char* ShadowDepth = "Shaders/Lighting/ShadowDepth.hlsl";
	inline constexpr const char* VSMBlur = "Shaders/Lighting/VSMBlur.hlsl";
	inline constexpr const char* ShadowMapVis = "Shaders/PostProcess/ShadowMapVis.hlsl";
	inline constexpr const char* Blit = "Shaders/PostProcess/Blit.hlsl";
	inline constexpr const char* CameraFade = "Shaders/PostProcess/CameraFade.hlsl";
	inline constexpr const char* CameraVignette = "Shaders/PostProcess/CameraVignette.hlsl";
	inline constexpr const char* CameraLetterbox = "Shaders/PostProcess/CameraLetterbox.hlsl";

	inline constexpr const char* BloomExtract = "Shaders/PostProcess/BloomExtract.hlsl";
	inline constexpr const char* BloomBlur = "Shaders/PostProcess/BloomBlur.hlsl";
	inline constexpr const char* BloomComposite = "Shaders/PostProcess/BloomComposite.hlsl";
	inline constexpr const char* DepthOfField = "Shaders/PostProcess/DepthOfField.hlsl";
}

namespace EShadowDepthDefines
{
	namespace EntryPoint
	{
		inline constexpr const char* StaticMeshVS = "VS_StaticMesh";
		inline constexpr const char* SkeletalMeshVS = "VS_SkeletalMesh";
		inline constexpr const char* PS = "PS";
	}

	enum class EVertexFactory : uint8
	{
		StaticMesh,
		SkeletalMesh,
		Count,
	};

	// StaticMesh: 매크로 없음 (기본 경로)
	inline const D3D_SHADER_MACRO StaticMesh[] = { {nullptr, nullptr} };
	// SkeletalMesh: 별도 VS 엔트리포인트가 GPU skinning 경로를 선택한다.
	inline const D3D_SHADER_MACRO SkeletalMesh[] = { {nullptr, nullptr} };

	inline FShaderKey MakePermutationKey(EVertexFactory VF)
	{
		const D3D_SHADER_MACRO* Defines = 
			(VF == EVertexFactory::SkeletalMesh) ? SkeletalMesh : StaticMesh;
		const char* VSEntry =
			(VF == EVertexFactory::SkeletalMesh) ? EntryPoint::SkeletalMeshVS :
			EntryPoint::StaticMeshVS;
		return FShaderKey(EShaderPath::ShadowDepth, Defines, VSEntry, EntryPoint::PS);
	}

}

namespace EPreDepthDefines
{
	namespace EntryPoint
	{
		inline constexpr const char* StaticMeshVS = "VS_StaticMesh";
		inline constexpr const char* SkeletalMeshVS = "VS_SkeletalMesh";
		inline constexpr const char* InstancedStaticMeshVS = "VS_InstancedStaticMesh";
		inline constexpr const char* PS = "PS";
	}

	enum class EVertexFactory : uint8
	{
		StaticMesh,
		SkeletalMesh,
		InstancedStaticMesh,
		Count,
	};

	inline const D3D_SHADER_MACRO StaticMesh[] = { {nullptr, nullptr} };
	inline const D3D_SHADER_MACRO SkeletalMesh[] = { {nullptr, nullptr} };
	inline const D3D_SHADER_MACRO InstancedStaticMesh[] = { {nullptr, nullptr} };

	inline FShaderKey MakePermutationKey(EVertexFactory VF)
	{
		const D3D_SHADER_MACRO* Defines =
			(VF == EVertexFactory::SkeletalMesh) ? SkeletalMesh :
			(VF == EVertexFactory::InstancedStaticMesh) ? InstancedStaticMesh :
			StaticMesh;
		const char* VSEntry =
			(VF == EVertexFactory::SkeletalMesh) ? EntryPoint::SkeletalMeshVS :
			(VF == EVertexFactory::InstancedStaticMesh) ? EntryPoint::InstancedStaticMeshVS :
			EntryPoint::StaticMeshVS;
		return FShaderKey(EShaderPath::PreDepth, Defines, VSEntry, EntryPoint::PS);
	}
}
namespace EUberLitDefines
{
	namespace EntryPoint
	{
		inline constexpr const char* StaticMeshVS = "VS_StaticMesh";
		inline constexpr const char* SkeletalMeshVS = "VS_SkeletalMesh";
		inline constexpr const char* InstancedStaticMeshVS = "VS_InstancedStaticMesh";
		inline constexpr const char* PS = "PS";
	}

	enum class ELightingModel : uint8
	{
		Default,
		Unlit,
		Gouraud,
		Lambert,
		Phong,
		Count,
	};

	enum class EVertexFactory : uint8
	{
		StaticMesh,
		SkeletalMesh,
		InstancedStaticMesh,
		Count,
	};

	inline const D3D_SHADER_MACRO Default[] = { {"LIGHTING_MODEL_PHONG", "1"}, {nullptr, nullptr} };
	inline const D3D_SHADER_MACRO Unlit[] = { {"LIGHTING_MODEL_UNLIT", "1"}, {nullptr, nullptr} };
	inline const D3D_SHADER_MACRO Gouraud[] = { {"LIGHTING_MODEL_GOURAUD", "1"}, {nullptr, nullptr} };
	inline const D3D_SHADER_MACRO Lambert[] = { {"LIGHTING_MODEL_LAMBERT", "1"}, {nullptr, nullptr} };
	inline const D3D_SHADER_MACRO Phong[] = { {"LIGHTING_MODEL_PHONG", "1"}, {nullptr, nullptr} };
	inline const D3D_SHADER_MACRO DefaultWeightBoneHeatMap[] = { {"LIGHTING_MODEL_PHONG", "1"}, {"WEIGHT_BONE_HEATMAP", "1"}, {nullptr, nullptr} };
	inline const D3D_SHADER_MACRO UnlitWeightBoneHeatMap[] = { {"LIGHTING_MODEL_UNLIT", "1"}, {"WEIGHT_BONE_HEATMAP", "1"}, {nullptr, nullptr} };
	inline const D3D_SHADER_MACRO GouraudWeightBoneHeatMap[] = { {"LIGHTING_MODEL_GOURAUD", "1"}, {"WEIGHT_BONE_HEATMAP", "1"}, {nullptr, nullptr} };
	inline const D3D_SHADER_MACRO LambertWeightBoneHeatMap[] = { {"LIGHTING_MODEL_LAMBERT", "1"}, {"WEIGHT_BONE_HEATMAP", "1"}, {nullptr, nullptr} };
	inline const D3D_SHADER_MACRO PhongWeightBoneHeatMap[] = { {"LIGHTING_MODEL_PHONG", "1"}, {"WEIGHT_BONE_HEATMAP", "1"}, {nullptr, nullptr} };
	inline const D3D_SHADER_MACRO DefaultFog[] = { {"LIGHTING_MODEL_PHONG", "1"}, {"APPLY_FOG", "1"}, {nullptr, nullptr} };
	inline const D3D_SHADER_MACRO UnlitFog[] = { {"LIGHTING_MODEL_UNLIT", "1"}, {"APPLY_FOG", "1"}, {nullptr, nullptr} };
	inline const D3D_SHADER_MACRO GouraudFog[] = { {"LIGHTING_MODEL_GOURAUD", "1"}, {"APPLY_FOG", "1"}, {nullptr, nullptr} };
	inline const D3D_SHADER_MACRO LambertFog[] = { {"LIGHTING_MODEL_LAMBERT", "1"}, {"APPLY_FOG", "1"}, {nullptr, nullptr} };
	inline const D3D_SHADER_MACRO PhongFog[] = { {"LIGHTING_MODEL_PHONG", "1"}, {"APPLY_FOG", "1"}, {nullptr, nullptr} };

	inline const D3D_SHADER_MACRO* GetDefines(ELightingModel LightingModel, EVertexFactory VertexFactory,
		bool bWeightBoneHeatMap = false, bool bApplyFog = false)
	{
		(void)VertexFactory;
		if (bWeightBoneHeatMap)
		{
			switch (LightingModel)
			{
			case ELightingModel::Unlit:   return UnlitWeightBoneHeatMap;
			case ELightingModel::Gouraud: return GouraudWeightBoneHeatMap;
			case ELightingModel::Lambert: return LambertWeightBoneHeatMap;
			case ELightingModel::Phong:   return PhongWeightBoneHeatMap;
			case ELightingModel::Default:
			default:                      return DefaultWeightBoneHeatMap;
			}
		}

		if (bApplyFog)
		{
			switch (LightingModel)
			{
			case ELightingModel::Unlit:   return UnlitFog;
			case ELightingModel::Gouraud: return GouraudFog;
			case ELightingModel::Lambert: return LambertFog;
			case ELightingModel::Phong:   return PhongFog;
			case ELightingModel::Default:
			default:                      return DefaultFog;
			}
		}

		switch (LightingModel)
		{
		case ELightingModel::Unlit:   return Unlit;
		case ELightingModel::Gouraud: return Gouraud;
		case ELightingModel::Lambert: return Lambert;
		case ELightingModel::Phong:   return Phong;
		case ELightingModel::Default:
		default:                      return Default;
		}
	}

	inline FShaderKey MakePermutationKey(ELightingModel LightingModel, EVertexFactory VertexFactory, 
		bool bWeightBoneHeatMap = false, bool bApplyFog = false)
	{
		const char* VSEntryPoint =
			VertexFactory == EVertexFactory::SkeletalMesh ? EntryPoint::SkeletalMeshVS :
			VertexFactory == EVertexFactory::InstancedStaticMesh ? EntryPoint::InstancedStaticMeshVS :
			EntryPoint::StaticMeshVS;
		return FShaderKey(EShaderPath::UberLit,
			GetDefines(LightingModel, VertexFactory, bWeightBoneHeatMap, bApplyFog),
			VSEntryPoint, EntryPoint::PS);
	}
}

// 셰이더별 저장된 매크로 정보 (핫 리로드 시 재컴파일에 사용)
struct FShaderCacheEntry
{
	std::unique_ptr<FShader> Shader;
	TArray<D3D_SHADER_MACRO> StoredDefines;  // 마지막 원소는 {nullptr,nullptr}
	TArray<FString> Includes;                // include 의존성 (Shaders/ 기준 상대 경로)
};

// CS 캐시 키: Path + EntryPoint
struct FCSKey
{
	FString Path;
	FString EntryPoint;

	bool operator==(const FCSKey& Other) const
	{
		return Path == Other.Path && EntryPoint == Other.EntryPoint;
	}
};

namespace std
{
	template<> struct hash<FCSKey>
	{
		size_t operator()(const FCSKey& K) const
		{
			size_t H1 = std::hash<FString>{}(K.Path);
			size_t H2 = std::hash<FString>{}(K.EntryPoint);
			return H1 ^ (H2 * 0x9e3779b97f4a7c15ULL);
		}
	};
}

// CS 캐시 엔트리
struct FCSCacheEntry
{
	std::unique_ptr<FComputeShader> Shader;
	TArray<FString> Includes;
};

class FShaderManager : public TSingleton<FShaderManager>
{
	friend class TSingleton<FShaderManager>;

public:
	void Initialize(ID3D11Device* InDevice);
	void Release();

	FShader* GetOrCreate(const FShaderKey& Key, EShaderErrorMode ErrorMode = EShaderErrorMode::Notification);
	FShader* PreCompile(const FShaderKey& Key, const D3D_SHADER_MACRO* Defines, EShaderErrorMode ErrorMode = EShaderErrorMode::Notification);
	FShader* GetOrCreate(const FString& Path, EShaderErrorMode ErrorMode = EShaderErrorMode::Notification) { return GetOrCreate(FShaderKey(Path), ErrorMode); }
	FShader* GetOrCreateShadowDepthPermutation(EShadowDepthDefines::EVertexFactory VF, EShaderErrorMode ErrorMode = EShaderErrorMode::Notification);
	FShader* GetOrCreatePreDepthPermutation(EPreDepthDefines::EVertexFactory VF, EShaderErrorMode ErrorMode = EShaderErrorMode::Notification);
	FShader* GetOrCreateUberLitPermutation(EUberLitDefines::ELightingModel LightingModel, EUberLitDefines::EVertexFactory VertexFactory,
		EShaderErrorMode ErrorMode = EShaderErrorMode::Notification, bool bWeightBoneHeatMap = false, bool bApplyFog = false);
	FShader* FindOrCreate(const FString& Path);

	// Compute Shader — 캐시 기반. 호출자는 포인터만 보관, FShaderManager가 소유 + 핫 리로드.
	FComputeShader* GetOrCreateCS(const FString& Path, const FString& EntryPoint);

private:
	FShaderManager() = default;

	// 셰이더 핫 리로드
	void OnShadersChanged(const TSet<FString>& ChangedFiles);
	void RebuildIncludeDependents();
	static TArray<D3D_SHADER_MACRO> CopyDefines(const D3D_SHADER_MACRO* Defines);

	static constexpr int32 UberLitLightingModelCount = static_cast<int32>(EUberLitDefines::ELightingModel::Count);
	static constexpr int32 UberLitVertexFactoryCount = static_cast<int32>(EUberLitDefines::EVertexFactory::Count);
	static constexpr int32 UberLitBoolCount = 2;
	static constexpr int32 ShadowDepthVertexFactoryCount = static_cast<int32>(EShadowDepthDefines::EVertexFactory::Count);
	static constexpr int32 PreDepthVertexFactoryCount = static_cast<int32>(EPreDepthDefines::EVertexFactory::Count);

	void ResetFastShaderPointerCaches();

	ID3D11Device* CachedDevice = nullptr;
	TMap<FShaderKey, FShaderCacheEntry> ShaderCache;
	TMap<FCSKey, FCSCacheEntry> CSCache;

	// DrawCommand collect 경로는 섹션마다 같은 shader permutation을 반복 조회한다.
	// unordered_map + FShaderKey 생성 비용을 피하기 위해 고정 permutation은 포인터 배열로 바로 반환한다.
	FShader* UberLitPermutationCache[UberLitLightingModelCount][UberLitVertexFactoryCount][UberLitBoolCount][UberLitBoolCount] = {};
	FShader* ShadowDepthPermutationCache[ShadowDepthVertexFactoryCount] = {};
	FShader* PreDepthPermutationCache[PreDepthVertexFactoryCount] = {};

	bool bIsInitialized = false;

	// include 파일 → 이를 사용하는 셰이더 키 역매핑
	TMap<FString, TArray<FShaderKey>> IncludeDependents;
	// include 파일 → CS 캐시 키 역매핑
	TMap<FString, TArray<FCSKey>> CSIncludeDependents;

	FSubscriptionID WatchSub = 0;
};
