#include "ShaderManager.h"
#include "Platform/Paths.h"
#include "Core/Log.h"
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

// ============================================================
// Initialize — 시스템 셰이더 사전 컴파일 + 파일 감시 구독
// ============================================================
void FShaderManager::Initialize(ID3D11Device* InDevice)
{
	if (bIsInitialized) return;
	CachedDevice = InDevice;

	// 단순 셰이더 (매크로 없음)
	GetOrCreate(EShaderPath::Primitive);
	GetOrCreate(EShaderPath::Gizmo);
	GetOrCreate(EShaderPath::Editor);
	GetOrCreate(EShaderPath::Decal);
	GetOrCreate(EShaderPath::Outline);
	GetOrCreate(EShaderPath::SceneDepth);
	GetOrCreate(EShaderPath::SceneNormal);
	GetOrCreate(EShaderPath::FXAA);
	GetOrCreate(EShaderPath::Font);
	GetOrCreate(EShaderPath::OverlayFont);
	GetOrCreate(EShaderPath::SubUV);
	GetOrCreate(EShaderPath::Billboard);
	GetOrCreate(EShaderPath::HeightFog);

	// UberLit 기본은 Phong + Cluster Culling으로 컴파일한다.
	GetOrCreate(EShaderPath::UberLit);
	PreCompile(FShaderKey(EShaderPath::UberLit, EUberLitDefines::Unlit),   EUberLitDefines::Unlit);
	PreCompile(FShaderKey(EShaderPath::UberLit, EUberLitDefines::Gouraud), EUberLitDefines::Gouraud);
	PreCompile(FShaderKey(EShaderPath::UberLit, EUberLitDefines::Lambert), EUberLitDefines::Lambert);
	PreCompile(FShaderKey(EShaderPath::UberLit, EUberLitDefines::Phong),   EUberLitDefines::Phong);
	PreCompile(FShaderKey(EShaderPath::UberLit, EUberLitDefines::Toon),    EUberLitDefines::Toon);

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
	IncludeDependents.clear();

	CachedDevice = nullptr;
	bIsInitialized = false;
}

// ============================================================
// GetOrCreate — 캐시 히트 시 반환, 미스 시 컴파일
// ============================================================
FShader* FShaderManager::GetOrCreate(const FShaderKey& Key)
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

	// DefinesHash가 0이면 매크로 없음. UberLit만 기본 Cluster Culling define을 적용한다.
	if (Key.DefinesHash == 0)
	{
		const bool bIsUberLit = (Key.Path == EShaderPath::UberLit);
		const D3D_SHADER_MACRO* Defines = bIsUberLit ? EUberLitDefines::Default : nullptr;
		CacheEntry.Shader->Create(CachedDevice, WidePath.c_str(), "VS", "PS", Defines, &CacheEntry.Includes);
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
FShader* FShaderManager::PreCompile(const FShaderKey& Key, const D3D_SHADER_MACRO* Defines)
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
	CacheEntry.Shader->Create(CachedDevice, WidePath.c_str(), "VS", "PS", Defines, &CacheEntry.Includes);
	CacheEntry.StoredDefines = CopyDefines(Defines);

	auto* RawPtr = CacheEntry.Shader.get();
	ShaderCache.emplace(Key, std::move(CacheEntry));
	return RawPtr;
}

// ============================================================
// FindOrCreate — MaterialManager용: 경로로 셰이더 조회, 없으면 컴파일
// ============================================================
FShader* FShaderManager::FindOrCreate(const FString& Path)
{
	return GetOrCreate(Path);
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
			// IncFile은 Shaders/ 기준 상대 경로 (e.g. "Common/ConstantBuffers.hlsl")
			// 구독 콜백에선 "Shaders/Common/ConstantBuffers.hlsl" 형태로 수신
			FString FullIncPath = "Shaders/" + IncFile;
			IncludeDependents[FullIncPath].push_back(Key);
		}
	}
}

// ============================================================
// OnShadersChanged — 셰이더 핫 리로드 콜백 (메인 스레드에서 호출됨)
// ============================================================
void FShaderManager::OnShadersChanged(const TSet<FString>& ChangedFiles)
{
	if (!CachedDevice) return;

	// 리컴파일 대상 셰이더 키 수집
	TSet<FShaderKey> RecompileTargets;

	for (const FString& File : ChangedFiles)
	{
		// 1. 직접 셰이더 매칭 — ShaderCache의 키 Path와 비교
		for (auto& [Key, Entry] : ShaderCache)
		{
			if (Key.Path == File)
			{
				RecompileTargets.insert(Key);
			}
		}

		// 2. include 파일 → 부모 셰이더 수집
		auto It = IncludeDependents.find(File);
		if (It != IncludeDependents.end())
		{
			for (const FShaderKey& DepKey : It->second)
			{
				RecompileTargets.insert(DepKey);
			}
		}
	}

	if (RecompileTargets.empty()) return;

	UE_LOG("[ShaderHotReload] Recompiling %zu shader(s)...", RecompileTargets.size());

	for (const FShaderKey& Key : RecompileTargets)
	{
		auto It = ShaderCache.find(Key);
		if (It == ShaderCache.end()) continue;

		FShaderCacheEntry& Entry = It->second;
		std::wstring WidePath = FPaths::ToWide(Key.Path);

		// 새 셰이더 시도 컴파일
		auto NewShader = std::make_unique<FShader>();
		TArray<FString> NewIncludes;
		const D3D_SHADER_MACRO* Defines = Entry.StoredDefines.empty() ? nullptr : Entry.StoredDefines.data();
		NewShader->Create(CachedDevice, WidePath.c_str(), "VS", "PS", Defines, &NewIncludes);

		if (NewShader->IsValid())
		{
			// 성공: 기존 FShader 객체에 in-place move (raw 포인터 유지)
			*Entry.Shader = std::move(*NewShader);
			Entry.Includes = std::move(NewIncludes);
			UE_LOG("[ShaderHotReload] OK: %s", Key.Path.c_str());
		}
		else
		{
			// 실패: 기존 셰이더 유지
			UE_LOG("[ShaderHotReload] FAILED: %s (keeping previous version)", Key.Path.c_str());
		}
	}

	// 역매핑 재구축
	RebuildIncludeDependents();
}
