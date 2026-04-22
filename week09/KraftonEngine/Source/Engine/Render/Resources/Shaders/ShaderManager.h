#pragma once

#include "Core/Singleton.h"
#include "Core/CoreTypes.h"
#include "Render/RHI/D3D11/Shaders/GraphicsShaderProgram.h"
#include "Render/Resources/Shaders/ShaderDependencyUtils.h"

#include <memory>

using ShaderDependencyUtils::FShaderFileDependency;

/*
    ������ ���� ����ϴ� ���� ���̴� �����Դϴ�.
    Opaque, Decal, Fog, Gizmo ���� ���� ���̴��� ���⼭ �����մϴ�.
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
    ����� ���̴� ĳ�� �׸��Դϴ�.
    �����ϵ� ���̴��� ���� ���� ���� ���� ������ �Բ� �����մϴ�.
*/
struct FCustomShaderCacheEntry
{
    std::unique_ptr<FShader> Shader;
    FShaderFileDependency SourceFile;
};

/*
    ���� ���̴��� ����� ���̴��� ����/����/�ָ��ε��ϴ� �Ŵ����Դϴ�.
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
