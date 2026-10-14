#include "ShaderManager.h"
#include "Platform/Paths.h"
#include "Core/Logging/Log.h"
#include "Core/Logging/Notification.h"
#include <algorithm>

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

void FShaderManager::ResetFastShaderPointerCaches()
{
	for (auto& LightingArray : UberLitPermutationCache)
	{
		for (auto& VertexFactoryArray : LightingArray)
		{
			for (auto& HeatArray : VertexFactoryArray)
			{
				for (FShader*& Shader : HeatArray)
				{
					Shader = nullptr;
				}
			}
		}
	}

	for (FShader*& Shader : ShadowDepthPermutationCache)
	{
		Shader = nullptr;
	}

	for (FShader*& Shader : PreDepthPermutationCache)
	{
		Shader = nullptr;
	}
}

// ============================================================
// Initialize — 시스템 셰이더 사전 컴파일 + 파일 감시 구독
// ============================================================
void FShaderManager::Initialize(ID3D11Device* InDevice)
{
	if (bIsInitialized) return;
	CachedDevice = InDevice;
	ResetFastShaderPointerCaches();

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
	GetOrCreate(EShaderPath::BloomExtract, StartupError);
	GetOrCreate(EShaderPath::BloomBlur, StartupError);
	GetOrCreate(EShaderPath::BloomComposite, StartupError);
	GetOrCreate(FShaderKey(EShaderPath::DepthOfField, nullptr, "VS", "PS_Setup"), StartupError);
	GetOrCreate(FShaderKey(EShaderPath::DepthOfField, nullptr, "VS", "PS_Gather"), StartupError);
	GetOrCreate(FShaderKey(EShaderPath::DepthOfField, nullptr, "VS", "PS_Recombine"), StartupError);
	GetOrCreate(EShaderPath::GammaCorrection, StartupError);
	GetOrCreatePreDepthPermutation(EPreDepthDefines::EVertexFactory::StaticMesh, StartupError);
	GetOrCreatePreDepthPermutation(EPreDepthDefines::EVertexFactory::SkeletalMesh, StartupError);
	GetOrCreatePreDepthPermutation(EPreDepthDefines::EVertexFactory::InstancedStaticMesh, StartupError);
	GetOrCreateShadowDepthPermutation(EShadowDepthDefines::EVertexFactory::StaticMesh, StartupError);
	GetOrCreateShadowDepthPermutation(EShadowDepthDefines::EVertexFactory::SkeletalMesh, StartupError);
	GetOrCreate(EShaderPath::ShadowMapVis, StartupError);
	GetOrCreate(EShaderPath::CameraFade, StartupError);
	GetOrCreate(EShaderPath::CameraVignette, StartupError);
	GetOrCreate(EShaderPath::CameraLetterbox, StartupError);

	// UberLit 기본은 StaticMesh VS + Phong으로 컴파일한다. 나머지 ViewMode/VertexFactory 조합은 lazy compile.
	GetOrCreate(EShaderPath::UberLit, StartupError);
	GetOrCreateUberLitPermutation(EUberLitDefines::ELightingModel::Default, EUberLitDefines::EVertexFactory::SkeletalMesh, StartupError);

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
	ResetFastShaderPointerCaches();

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

	if (Key.Path == EShaderPath::PreDepth && Key.VSEntryPoint == "VS")
	{
		return GetOrCreatePreDepthPermutation(EPreDepthDefines::EVertexFactory::StaticMesh, ErrorMode);
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
		CacheEntry.Shader->Create(CachedDevice, WidePath.c_str(), Key.VSEntryPoint.c_str(), Key.PSEntryPoint.c_str(), Defines, &CacheEntry.Includes, ErrorMode);
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
	CacheEntry.Shader->Create(CachedDevice, WidePath.c_str(), Key.VSEntryPoint.c_str(), Key.PSEntryPoint.c_str(), Defines, &CacheEntry.Includes, ErrorMode);
	CacheEntry.StoredDefines = CopyDefines(Defines);

	auto* RawPtr = CacheEntry.Shader.get();
	ShaderCache.emplace(Key, std::move(CacheEntry));
	return RawPtr;
}

FShader* FShaderManager::GetOrCreateShadowDepthPermutation(EShadowDepthDefines::EVertexFactory VF, EShaderErrorMode ErrorMode)
{
	const int32 VFIndex = static_cast<int32>(VF);
	if (VFIndex >= 0 && VFIndex < ShadowDepthVertexFactoryCount)
	{
		if (FShader* Cached = ShadowDepthPermutationCache[VFIndex])
		{
			return Cached;
		}
	}

	const D3D_SHADER_MACRO* Defines =
		(VF == EShadowDepthDefines::EVertexFactory::SkeletalMesh)
		? EShadowDepthDefines::SkeletalMesh
		: EShadowDepthDefines::StaticMesh;
	FShader* Shader = PreCompile(EShadowDepthDefines::MakePermutationKey(VF), Defines, ErrorMode);

	if (VFIndex >= 0 && VFIndex < ShadowDepthVertexFactoryCount)
	{
		ShadowDepthPermutationCache[VFIndex] = Shader;
	}
	return Shader;
}

FShader* FShaderManager::GetOrCreatePreDepthPermutation(EPreDepthDefines::EVertexFactory VF, EShaderErrorMode ErrorMode)
{
	const int32 VFIndex = static_cast<int32>(VF);
	if (VFIndex >= 0 && VFIndex < PreDepthVertexFactoryCount)
	{
		if (FShader* Cached = PreDepthPermutationCache[VFIndex])
		{
			return Cached;
		}
	}

	const D3D_SHADER_MACRO* Defines =
		(VF == EPreDepthDefines::EVertexFactory::SkeletalMesh)
		? EPreDepthDefines::SkeletalMesh :
		(VF == EPreDepthDefines::EVertexFactory::InstancedStaticMesh)
		? EPreDepthDefines::InstancedStaticMesh :
		EPreDepthDefines::StaticMesh;
	FShader* Shader = PreCompile(EPreDepthDefines::MakePermutationKey(VF), Defines, ErrorMode);

	if (VFIndex >= 0 && VFIndex < PreDepthVertexFactoryCount)
	{
		PreDepthPermutationCache[VFIndex] = Shader;
	}
	return Shader;
}

FShader* FShaderManager::GetOrCreateUberLitPermutation(EUberLitDefines::ELightingModel LightingModel,
	EUberLitDefines::EVertexFactory VertexFactory, EShaderErrorMode ErrorMode, bool bWeightBoneHeatMap, bool bApplyFog)
{
	const int32 LIndex = static_cast<int32>(LightingModel);
	const int32 VFIndex = static_cast<int32>(VertexFactory);
	const int32 HeatIndex = bWeightBoneHeatMap ? 1 : 0;
	const int32 FogIndex = bApplyFog ? 1 : 0;

	if (LIndex >= 0 && LIndex < UberLitLightingModelCount &&
		VFIndex >= 0 && VFIndex < UberLitVertexFactoryCount)
	{
		if (FShader* Cached = UberLitPermutationCache[LIndex][VFIndex][HeatIndex][FogIndex])
		{
			return Cached;
		}
	}

	const D3D_SHADER_MACRO* Defines = EUberLitDefines::GetDefines(LightingModel, VertexFactory, bWeightBoneHeatMap, bApplyFog);
	FShader* Shader = PreCompile(EUberLitDefines::MakePermutationKey(LightingModel, VertexFactory, bWeightBoneHeatMap, bApplyFog), Defines, ErrorMode);

	if (LIndex >= 0 && LIndex < UberLitLightingModelCount &&
		VFIndex >= 0 && VFIndex < UberLitVertexFactoryCount)
	{
		UberLitPermutationCache[LIndex][VFIndex][HeatIndex][FogIndex] = Shader;
	}
	return Shader;
}

// ============================================================
// FindOrCreate — MaterialManager용: 경로로 셰이더 조회, 없으면 컴파일
// ============================================================
FShader* FShaderManager::FindOrCreate(const FString& Path)
{
	return GetOrCreate(Path);
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
		NewShader->Create(CachedDevice, WidePath.c_str(), Key.VSEntryPoint.c_str(), Key.PSEntryPoint.c_str(), Defines, &NewIncludes);

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
