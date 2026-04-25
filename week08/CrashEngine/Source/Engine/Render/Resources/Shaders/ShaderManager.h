#pragma once

#include "Core/Singleton.h"
#include "Core/CoreTypes.h"
#include "Render/Execute/Registry/ShaderProgramRegistry.h"
#include "Render/Execute/Registry/ShaderProgramTypes.h"
#include "Render/RHI/D3D11/Shaders/GraphicsProgram.h"
#include "Render/Resources/Shaders/ShaderDependencyUtils.h"

#include <memory>

using ShaderDependencyUtils::FShaderFileDependency;

/*
    에디터에서 임의로 요청한 셰이더 프로그램을 보관하는 캐시 항목입니다.
    원본 파일 의존성도 함께 저장해 디버그 빌드에서 핫 리로드 여부를 판단합니다.
*/
struct FCustomShaderCacheEntry
{
    std::unique_ptr<FGraphicsProgram> Shader;
    FShaderFileDependency             SourceFile;
};

// FShaderManager는 관련 객체의 생성, 조회, 수명 관리를 담당합니다.
class FShaderManager : public TSingleton<FShaderManager>
{
    friend class TSingleton<FShaderManager>;

public:
    void Initialize(ID3D11Device* InDevice);
    void Release();
    void TickHotReload();

    FGraphicsProgram* GetShader(EShaderType InType);
    FGraphicsProgram* GetCustomShader(const FString& Key);
    FGraphicsProgram* CreateCustomShader(ID3D11Device* InDevice, const wchar_t* InFilePath);

private:
    FShaderManager() = default;

    void RefreshBuiltInShader(EShaderType InType);
    bool RefreshCustomShader(FCustomShaderCacheEntry& Entry, const FString& NormalizedKey);

    FShaderProgramRegistry                 ShaderRegistry;
    FGraphicsProgram                       Shaders[(uint32)EShaderType::MAX];
    FShaderFileDependency                  BuiltInShaderFiles[(uint32)EShaderType::MAX];
    TMap<FString, FCustomShaderCacheEntry> CustomShaderCache;

    ID3D11Device* Device         = nullptr;
    bool          bIsInitialized = false;
};
