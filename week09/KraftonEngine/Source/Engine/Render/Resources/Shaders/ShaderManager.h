#pragma once

#include "Core/Singleton.h"
#include "Core/CoreTypes.h"
#include "Render/RHI/D3D11/Shaders/GraphicsShaderProgram.h"
#include "Render/Resources/Shaders/ShaderDependencyUtils.h"

#include <memory>

using ShaderDependencyUtils::FShaderFileDependency;

/*
    엔진이 자주 사용하는 내장 셰이더 종류입니다.
    BaseDraw, Decal, Fog, Gizmo 같은 공용 셰이더를 여기서 관리합니다.
*/
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
    DepthOnly,
    SceneDepth,
    NormalView,
    FXAA,
	LightHitMap,
    MAX,
};

/*
    사용자 셰이더 캐시 항목입니다.
    컴파일된 셰이더와 원본 파일 변경 감시 정보를 함께 보관합니다.
*/
struct FCustomShaderCacheEntry
{
    std::unique_ptr<FShader> Shader;
    FShaderFileDependency SourceFile;
};

/*
    내장 셰이더와 사용자 셰이더를 생성/보관/핫리로드하는 매니저입니다.
*/
class FShaderManager : public TSingleton<FShaderManager>
{
    friend class TSingleton<FShaderManager>;

public:
    void Initialize(ID3D11Device* InDevice);
    void Release();
    void TickHotReload();

    FShader* GetShader(EShaderType InType);
    FShader* GetCustomShader(const FString& Key);
    FShader* CreateCustomShader(ID3D11Device* InDevice, const wchar_t* InFilePath);

private:
    FShaderManager() = default;

    void RefreshBuiltInShader(EShaderType InType);
    bool RefreshCustomShader(FCustomShaderCacheEntry& Entry, const FString& NormalizedKey);
    FString GetBuiltInShaderPath(EShaderType InType) const;

    FShader Shaders[(uint32)EShaderType::MAX];
    FShaderFileDependency BuiltInShaderFiles[(uint32)EShaderType::MAX];
    TMap<FString, FCustomShaderCacheEntry> CustomShaderCache;

    ID3D11Device* Device = nullptr;
    bool bIsInitialized = false;
};
