#pragma once

#include "Core/Singleton.h"
#include "Core/CoreTypes.h"
#include "Render/D3D11/Shaders/Programs/GraphicsShaderProgram.h"
#include "Render/Resources/Shaders/ShaderDependencyUtils.h"

#include <memory>

using ShaderDependencyUtils::FShaderFileDependency;

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
    MAX,
};

struct FCustomShaderCacheEntry
{
    std::unique_ptr<FShader> Shader;
    FShaderFileDependency SourceFile;
};

class FShaderManager : public TSingleton<FShaderManager>
{
    friend class TSingleton<FShaderManager>;

public:
    void Initialize(ID3D11Device* InDevice);
    void Release();

    FShader* GetShader(EShaderType InType);
    FShader* GetCustomShader(const FString& Key);
    FShader* CreateCustomShader(ID3D11Device* InDevice, const wchar_t* InFilePath);

private:
    FShaderManager() = default;

    void RefreshBuiltInShader(EShaderType InType);
    FString GetBuiltInShaderPath(EShaderType InType) const;

    FShader Shaders[(uint32)EShaderType::MAX];
    FShaderFileDependency BuiltInShaderFiles[(uint32)EShaderType::MAX];
    TMap<FString, FCustomShaderCacheEntry> CustomShaderCache;
    TArray<std::unique_ptr<FShader>> RetiredCustomShaders;

    ID3D11Device* Device = nullptr;
    bool bIsInitialized = false;
};
