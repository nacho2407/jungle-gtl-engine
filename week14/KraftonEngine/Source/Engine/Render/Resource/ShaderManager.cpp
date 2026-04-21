#include "ShaderManager.h"
#include "Platform/Paths.h"

// ============================================================
// Initialize — 시스템 셰이더 사전 컴파일 (GetOrCreate로 캐시에 등록)
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

	bIsInitialized = true;
}

void FShaderManager::Release()
{
	for (auto& [Key, Shader] : ShaderCache)
	{
		Shader->Release();
	}
	ShaderCache.clear();

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
		return It->second.get();
	}

	if (!CachedDevice) return nullptr;

	auto NewShader = std::make_unique<FShader>();
	std::wstring WidePath = FPaths::ToWide(Key.Path);

	// DefinesHash가 0이면 매크로 없음. UberLit만 기본 Cluster Culling define을 적용한다.
	if (Key.DefinesHash == 0)
	{
		const bool bIsUberLit = (Key.Path == EShaderPath::UberLit);
		const D3D_SHADER_MACRO* Defines = bIsUberLit ? EUberLitDefines::Default : nullptr;
		NewShader->Create(CachedDevice, WidePath.c_str(), "VS", "PS", Defines);
	}
	else
	{
		// 매크로가 있는 셰이더는 Initialize에서 사전 컴파일되어야 함.
		// 런타임에 새 매크로 조합이 필요하면 해시만으로는 매크로를 복원할 수 없음.
		return nullptr;
	}

	auto* RawPtr = NewShader.get();
	ShaderCache.emplace(Key, std::move(NewShader));
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
		return It->second.get();
	}

	if (!CachedDevice) return nullptr;

	auto NewShader = std::make_unique<FShader>();
	std::wstring WidePath = FPaths::ToWide(Key.Path);
	NewShader->Create(CachedDevice, WidePath.c_str(), "VS", "PS", Defines);

	auto* RawPtr = NewShader.get();
	ShaderCache.emplace(Key, std::move(NewShader));
	return RawPtr;
}

// ============================================================
// FindOrCreate — MaterialManager용: 경로로 셰이더 조회, 없으면 컴파일
// ============================================================
FShader* FShaderManager::FindOrCreate(const FString& Path)
{
	return GetOrCreate(Path);
}
