#include "ShaderManager.h"
#include "Platform/Paths.h"
#include "Core/Logging/Log.h"
#include "Core/Logging/Notification.h"
#include <algorithm>

namespace
{
	const D3D11_INPUT_ELEMENT_DESC ParticleSpriteInputLayout[] =
	{
		{ "TEXCOORD",              0, DXGI_FORMAT_R32G32_FLOAT,       0, 0,                            D3D11_INPUT_PER_VERTEX_DATA,   0 },
		{ "INSTANCE_POSITION",     0, DXGI_FORMAT_R32G32B32_FLOAT,    1, 0,                            D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "INSTANCE_SIZE",         0, DXGI_FORMAT_R32_FLOAT,          1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "INSTANCE_COLOR",        0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "INSTANCE_ROTATION",     0, DXGI_FORMAT_R32_FLOAT,          1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "INSTANCE_SUBIMAGE",     0, DXGI_FORMAT_R32_FLOAT,          1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "INSTANCE_DYNAMICPARAM", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
	};

	const D3D11_INPUT_ELEMENT_DESC ParticleMeshInputLayout[] =
	{
		{ "POSITION",              0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,                            D3D11_INPUT_PER_VERTEX_DATA,   0 },
		{ "NORMAL",                0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA,   0 },
		{ "COLOR",                 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA,   0 },
		{ "TEXTCOORD",             0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA,   0 },
		{ "INSTANCE_TRANSFORM",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0,                            D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "INSTANCE_TRANSFORM",    1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "INSTANCE_TRANSFORM",    2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "INSTANCE_TRANSFORM",    3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "INSTANCE_COLOR",        0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "INSTANCE_SUBIMAGE",     0, DXGI_FORMAT_R32_FLOAT,          1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "INSTANCE_DYNAMICPARAM", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
	};

	const FShaderInputLayoutDesc ParticleSpriteLayoutDesc =
	{
		ParticleSpriteInputLayout,
		static_cast<uint32>(sizeof(ParticleSpriteInputLayout) / sizeof(ParticleSpriteInputLayout[0]))
	};

	const FShaderInputLayoutDesc ParticleMeshLayoutDesc =
	{
		ParticleMeshInputLayout,
		static_cast<uint32>(sizeof(ParticleMeshInputLayout) / sizeof(ParticleMeshInputLayout[0]))
	};

	const D3D11_INPUT_ELEMENT_DESC ParticleBeamTrailInputLayout[] =
	{
		{ "POSITION",       0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,                            D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "RELATIVE_TIME",  0, DXGI_FORMAT_R32_FLOAT,          0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "OLD_POSITION",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "PARTICLE_ID",    0, DXGI_FORMAT_R32_FLOAT,          0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "SIZE",           0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "ROTATION",       0, DXGI_FORMAT_R32_FLOAT,          0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "SUBIMAGE_INDEX", 0, DXGI_FORMAT_R32_FLOAT,          0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",          0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD",       0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD",       1, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	const FShaderInputLayoutDesc ParticleBeamTrailLayoutDesc =
	{
		ParticleBeamTrailInputLayout,
		static_cast<uint32>(sizeof(ParticleBeamTrailInputLayout) / sizeof(ParticleBeamTrailInputLayout[0]))
	};

	const FShaderInputLayoutDesc* GetExplicitInputLayoutDesc(const FShaderKey& Key)
	{
		switch (Key.VertexFactory)
		{
		case EShaderVertexFactory::ParticleSprite:
			return &ParticleSpriteLayoutDesc;
		case EShaderVertexFactory::ParticleMesh:
			return &ParticleMeshLayoutDesc;
		default:
			break;
		}

		if (Key.Path == EShaderPath::ParticleSprite)
		{
			return &ParticleSpriteLayoutDesc;
		}
		if (Key.Path == EShaderPath::ParticleMesh)
		{
			return &ParticleMeshLayoutDesc;
		}
		if (Key.Path == EShaderPath::ParticleBeamTrail)
		{
			return &ParticleBeamTrailLayoutDesc;
		}
		return nullptr;
	}
}

// ============================================================
// CopyDefines — D3D_SHADER_MACRO 배열을 소유 가능한 형태로 복사
// ============================================================
TArray<D3D_SHADER_MACRO> FShaderManager::CopyDefines(const D3D_SHADER_MACRO* Defines)
{
	TArray<D3D_SHADER_MACRO> Result;
	if (!Defines) return Result;

	for (const D3D_SHADER_MACRO* D = Defines; D->Name != nullptr; ++D)
	{
		Result.push_back({ D->Name, D->Definition });
	}
	Result.push_back({ nullptr, nullptr });
	return Result;
}

// ============================================================
// Initialize — 시스템 셰이더 사전 컴파일 + 파일 감시 구독
// ============================================================
void FShaderManager::Initialize(ID3D11Device* InDevice)
{
	if (bIsInitialized) return;
	CachedDevice = InDevice;

	// 단순 셰이더 (매크로 없음) — 첫 시작이므로 MessageBox로 에러 표시
	constexpr EShaderErrorMode StartupError = EShaderErrorMode::MessageBox;

	GetOrCreate(EShaderPath::Primitive, StartupError);
	GetOrCreate(EShaderPath::Gizmo, StartupError);
	GetOrCreate(EShaderPath::Editor, StartupError);
	GetOrCreate(EShaderPath::Decal, StartupError);
	GetOrCreate(EShaderPath::Outline, StartupError);
	GetOrCreate(EShaderPath::SceneDepth, StartupError);
	GetOrCreate(EShaderPath::SceneNormal, StartupError);
	GetOrCreate(EShaderPath::FXAA, StartupError);
	GetOrCreate(EShaderPath::Font, StartupError);
	GetOrCreate(EShaderPath::OverlayFont, StartupError);
	GetOrCreate(EShaderPath::SubUV, StartupError);
	GetOrCreate(EShaderPath::Billboard, StartupError);
	GetOrCreate(EShaderPath::HeightFog, StartupError);
	GetOrCreate(EShaderPath::DOFDownSampling, StartupError);
	GetOrCreate(EShaderPath::DOFBlur, StartupError);
	GetOrCreate(EShaderPath::DOFBokeh, StartupError);
	GetOrCreate(EShaderPath::DOFComposite, StartupError);
	GetOrCreate(EShaderPath::GammaCorrection, StartupError);
	GetOrCreateShadowDepthPermutation(EShadowDepthDefines::EVertexFactory::StaticMesh, StartupError);
	GetOrCreateShadowDepthPermutation(EShadowDepthDefines::EVertexFactory::SkeletalMesh, StartupError);
	GetOrCreate(EShaderPath::ShadowMapVis, StartupError);
	GetOrCreate(EShaderPath::CameraFade, StartupError);
	GetOrCreate(EShaderPath::CameraVignette, StartupError);
	GetOrCreate(EShaderPath::CameraLetterbox, StartupError);

	// UberLit 기본은 StaticMesh VS + Phong으로 컴파일한다. 나머지 ViewMode/VertexFactory 조합은 lazy compile.
	GetOrCreate(EShaderPath::UberLit, StartupError);
	GetOrCreateUberLitPermutation(EUberLitDefines::ELightingModel::Default, EUberLitDefines::EVertexFactory::SkeletalMesh, StartupError);

	// AlphaBlend 패스용 — USE_FOG=1 퍼뮤테이션 사전 컴파일
	GetOrCreateUberLitPermutation(EUberLitDefines::ELightingModel::Default, EUberLitDefines::EVertexFactory::StaticMesh,   StartupError, false, true);
	GetOrCreateUberLitPermutation(EUberLitDefines::ELightingModel::Default, EUberLitDefines::EVertexFactory::SkeletalMesh, StartupError, false, true);

	// include 역매핑 구축
	RebuildIncludeDependents();

	// 셰이더 디렉토리 감시 등록
	FWatchID WatchID = FDirectoryWatcher::Get().Watch(FPaths::ShaderDir(), "Shaders/");
	if (WatchID != 0)
	{
		WatchSub = FDirectoryWatcher::Get().Subscribe(WatchID,
			[this](const TSet<FString>& Files) { OnShadersChanged(Files); });
	}

	bIsInitialized = true;
}

void FShaderManager::Release()
{
	if (WatchSub != 0)
	{
		FDirectoryWatcher::Get().Unsubscribe(WatchSub);
		WatchSub = 0;
	}

	for (auto& [Key, Entry] : ShaderCache)
	{
		Entry.Shader->Release();
	}
	ShaderCache.clear();
	CSCache.clear();
	IncludeDependents.clear();
	CSIncludeDependents.clear();

	CachedDevice = nullptr;
	bIsInitialized = false;
}

// ============================================================
// GetOrCreate — 캐시 히트 시 반환, 미스 시 컴파일
// ============================================================
FShader* FShaderManager::GetOrCreate(const FShaderKey& Key, EShaderErrorMode ErrorMode)
{
	if (Key.Path == EShaderPath::UberLit && Key.DefinesHash == 0)
	{
		return GetOrCreateUberLitPermutation(EUberLitDefines::ELightingModel::Default,
			EUberLitDefines::EVertexFactory::StaticMesh, ErrorMode);
	}

	if (Key.Path == EShaderPath::ShadowDepth && Key.VSEntryPoint == "VS")
	{
		return GetOrCreateShadowDepthPermutation(EShadowDepthDefines::EVertexFactory::StaticMesh, ErrorMode);
	}

	auto It = ShaderCache.find(Key);
	if (It != ShaderCache.end())
	{
		return It->second.Shader.get();
	}

	if (!CachedDevice) return nullptr;

	FShaderCacheEntry CacheEntry;
	CacheEntry.Shader = std::make_unique<FShader>();
	std::wstring WidePath = FPaths::ToWide(Key.Path);

	// DefinesHash가 0이면 매크로 없음.
	if (Key.DefinesHash == 0)
	{
		const D3D_SHADER_MACRO* Defines = nullptr;
		CacheEntry.Shader->Create(CachedDevice, WidePath.c_str(), Key.VSEntryPoint.c_str(), Key.PSEntryPoint.c_str(), Defines,
			&CacheEntry.Includes, ErrorMode, GetExplicitInputLayoutDesc(Key));
		CacheEntry.StoredDefines = CopyDefines(Defines);
	}
	else
	{
		// 매크로가 있는 셰이더는 Initialize에서 사전 컴파일되어야 함.
		return nullptr;
	}

	auto* RawPtr = CacheEntry.Shader.get();
	ShaderCache.emplace(Key, std::move(CacheEntry));
	return RawPtr;
}

// ============================================================
// PreCompile — 매크로 포함 셰이더 사전 컴파일
// ============================================================
FShader* FShaderManager::PreCompile(const FShaderKey& Key, const D3D_SHADER_MACRO* Defines, EShaderErrorMode ErrorMode)
{
	auto It = ShaderCache.find(Key);
	if (It != ShaderCache.end())
	{
		return It->second.Shader.get();
	}

	if (!CachedDevice) return nullptr;

	FShaderCacheEntry CacheEntry;
	CacheEntry.Shader = std::make_unique<FShader>();
	std::wstring WidePath = FPaths::ToWide(Key.Path);
	CacheEntry.Shader->Create(CachedDevice, WidePath.c_str(), Key.VSEntryPoint.c_str(), Key.PSEntryPoint.c_str(), Defines,
		&CacheEntry.Includes, ErrorMode, GetExplicitInputLayoutDesc(Key));
	CacheEntry.StoredDefines = CopyDefines(Defines);

	auto* RawPtr = CacheEntry.Shader.get();
	ShaderCache.emplace(Key, std::move(CacheEntry));
	return RawPtr;
}

FShader* FShaderManager::GetOrCreateShadowDepthPermutation(EShadowDepthDefines::EVertexFactory VF, EShaderErrorMode ErrorMode)
{
	const D3D_SHADER_MACRO* Defines =
		(VF == EShadowDepthDefines::EVertexFactory::SkeletalMesh)
		? EShadowDepthDefines::SkeletalMesh
		: EShadowDepthDefines::StaticMesh;
	return PreCompile(EShadowDepthDefines::MakePermutationKey(VF), Defines, ErrorMode);
}

FShader* FShaderManager::GetOrCreateUberLitPermutation(EUberLitDefines::ELightingModel LightingModel,
	EUberLitDefines::EVertexFactory VertexFactory, EShaderErrorMode ErrorMode, bool bWeightBoneHeatMap, bool bFog)
{
	const D3D_SHADER_MACRO* Defines = EUberLitDefines::GetDefines(LightingModel, VertexFactory, bWeightBoneHeatMap, bFog);
	return PreCompile(EUberLitDefines::MakePermutationKey(LightingModel, VertexFactory, bWeightBoneHeatMap, bFog), Defines, ErrorMode);
}

// ============================================================
// FindOrCreate — MaterialManager용: 경로로 셰이더 조회, 없으면 컴파일
// ============================================================
FShader* FShaderManager::FindOrCreate(const FString& Path)
{
	return GetOrCreate(Path);
}

FShader* FShaderManager::FindOrCreate(const FShaderKey& Key)
{
	return GetOrCreate(Key);
}

void FShaderManager::InvalidatePath(const FString& Path)
{
	for (auto It = ShaderCache.begin(); It != ShaderCache.end();)
	{
		if (It->first.Path == Path)
		{
			if (It->second.Shader)
			{
				It->second.Shader->Release();
			}
			It = ShaderCache.erase(It);
		}
		else
		{
			++It;
		}
	}

	RebuildIncludeDependents();
}

// ============================================================
// GetOrCreateCS — CS 캐시 히트 시 반환, 미스 시 컴파일
// ============================================================
FComputeShader* FShaderManager::GetOrCreateCS(const FString& Path, const FString& EntryPoint)
{
	FCSKey Key{ Path, EntryPoint };

	auto It = CSCache.find(Key);
	if (It != CSCache.end())
	{
		return It->second.Shader.get();
	}

	if (!CachedDevice) return nullptr;

	FCSCacheEntry CacheEntry;
	CacheEntry.Shader = std::make_unique<FComputeShader>();
	std::wstring WidePath = FPaths::ToWide(Path);
	CacheEntry.Shader->Create(CachedDevice, WidePath.c_str(), EntryPoint.c_str(), &CacheEntry.Includes);

	auto* RawPtr = CacheEntry.Shader.get();
	CSCache.emplace(Key, std::move(CacheEntry));

	RebuildIncludeDependents();
	return RawPtr;
}

// ============================================================
// RebuildIncludeDependents — include 역매핑 재구축
// ============================================================
void FShaderManager::RebuildIncludeDependents()
{
	IncludeDependents.clear();
	for (auto& [Key, Entry] : ShaderCache)
	{
		for (const FString& IncFile : Entry.Includes)
		{
			FString FullIncPath = "Shaders/" + IncFile;
			IncludeDependents[FullIncPath].push_back(Key);
		}
	}

	CSIncludeDependents.clear();
	for (auto& [Key, Entry] : CSCache)
	{
		for (const FString& IncFile : Entry.Includes)
		{
			FString FullIncPath = "Shaders/" + IncFile;
			CSIncludeDependents[FullIncPath].push_back(Key);
		}
	}
}

// ============================================================
// OnShadersChanged — 셰이더 핫 리로드 콜백 (메인 스레드에서 호출됨)
// ============================================================
void FShaderManager::OnShadersChanged(const TSet<FString>& ChangedFiles)
{
	if (!CachedDevice) return;

	// VS+PS 리컴파일 대상 수집
	TSet<FShaderKey> RecompileTargets;
	// CS 리컴파일 대상 수집
	TSet<FCSKey> CSRecompileTargets;

	for (const FString& File : ChangedFiles)
	{
		// 1. VS+PS 직접 매칭
		for (auto& [Key, Entry] : ShaderCache)
		{
			if (Key.Path == File)
			{
				RecompileTargets.insert(Key);
			}
		}

		// 2. VS+PS include 역매핑
		auto It = IncludeDependents.find(File);
		if (It != IncludeDependents.end())
		{
			for (const FShaderKey& DepKey : It->second)
			{
				RecompileTargets.insert(DepKey);
			}
		}

		// 3. CS 직접 매칭
		for (auto& [Key, Entry] : CSCache)
		{
			if (Key.Path == File)
			{
				CSRecompileTargets.insert(Key);
			}
		}

		// 4. CS include 역매핑
		auto CSIt = CSIncludeDependents.find(File);
		if (CSIt != CSIncludeDependents.end())
		{
			for (const FCSKey& DepKey : CSIt->second)
			{
				CSRecompileTargets.insert(DepKey);
			}
		}
	}

	size_t TotalTargets = RecompileTargets.size() + CSRecompileTargets.size();
	if (TotalTargets == 0) return;

	UE_LOG("[ShaderHotReload] Recompiling %zu shader(s)...", TotalTargets);

	// VS+PS 리컴파일
	for (const FShaderKey& Key : RecompileTargets)
	{
		auto It = ShaderCache.find(Key);
		if (It == ShaderCache.end()) continue;

		FShaderCacheEntry& Entry = It->second;
		std::wstring WidePath = FPaths::ToWide(Key.Path);

		auto NewShader = std::make_unique<FShader>();
		TArray<FString> NewIncludes;
		const D3D_SHADER_MACRO* Defines = Entry.StoredDefines.empty() ? nullptr : Entry.StoredDefines.data();
		NewShader->Create(CachedDevice, WidePath.c_str(), Key.VSEntryPoint.c_str(), Key.PSEntryPoint.c_str(), Defines,
			&NewIncludes, EShaderErrorMode::Notification, GetExplicitInputLayoutDesc(Key));

		if (NewShader->IsValid())
		{
			*Entry.Shader = std::move(*NewShader);
			Entry.Includes = std::move(NewIncludes);
			UE_LOG("[ShaderHotReload] OK: %s", Key.Path.c_str());
			FNotificationManager::Get().AddNotification("Shader Recompiled: " + Key.Path, ENotificationType::Success, 3.0f);
		}
		else
		{
			UE_LOG("[ShaderHotReload] FAILED: %s (keeping previous version)", Key.Path.c_str());
			FNotificationManager::Get().AddNotification("Shader Failed: " + Key.Path, ENotificationType::Error, 5.0f);
		}
	}

	// CS 리컴파일
	for (const FCSKey& Key : CSRecompileTargets)
	{
		auto It = CSCache.find(Key);
		if (It == CSCache.end()) continue;

		FCSCacheEntry& Entry = It->second;
		std::wstring WidePath = FPaths::ToWide(Key.Path);

		auto NewCS = std::make_unique<FComputeShader>();
		TArray<FString> NewIncludes;
		NewCS->Create(CachedDevice, WidePath.c_str(), Key.EntryPoint.c_str(), &NewIncludes);

		if (NewCS->IsValid())
		{
			// Detach로 NewCS에서 소유권 분리 후 Swap으로 Entry에 이전
			Entry.Shader->Swap(NewCS->Detach());
			Entry.Includes = std::move(NewIncludes);
			UE_LOG("[ShaderHotReload] CS OK: %s (%s)", Key.Path.c_str(), Key.EntryPoint.c_str());
			FNotificationManager::Get().AddNotification("CS Recompiled: " + Key.Path, ENotificationType::Success, 3.0f);
		}
		else
		{
			UE_LOG("[ShaderHotReload] CS FAILED: %s (keeping previous version)", Key.Path.c_str());
			FNotificationManager::Get().AddNotification("CS Failed: " + Key.Path, ENotificationType::Error, 5.0f);
		}
	}

	// 역매핑 재구축
	RebuildIncludeDependents();
}
