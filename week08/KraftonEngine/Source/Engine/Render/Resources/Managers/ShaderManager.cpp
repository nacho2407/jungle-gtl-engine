#include "Render/Resources/Managers/ShaderManager.h"

#include "Platform/Paths.h"

void FShaderManager::Initialize(ID3D11Device* InDevice)
{
    Device = InDevice;
    if (!bIsInitialized)
    {
        bIsInitialized = true;
    }

    for (uint32 i = 0; i < (uint32)EShaderType::MAX; ++i)
    {
        RefreshBuiltInShader(static_cast<EShaderType>(i));
    }
}

void FShaderManager::Release()
{
    for (uint32 i = 0; i < (uint32)EShaderType::MAX; ++i)
    {
        Shaders[i].Release();
        BuiltInShaderFiles[i] = {};
    }

    for (auto& [Key, Entry] : CustomShaderCache)
    {
        if (Entry.Shader)
        {
            Entry.Shader->Release();
        }
    }
    CustomShaderCache.clear();

    for (auto& Shader : RetiredCustomShaders)
    {
        if (Shader)
        {
            Shader->Release();
        }
    }
    RetiredCustomShaders.clear();

    Device = nullptr;
    bIsInitialized = false;
}

FShader* FShaderManager::GetShader(EShaderType InType)
{
    const uint32 Idx = (uint32)InType;
    if (Idx >= (uint32)EShaderType::MAX)
    {
        return nullptr;
    }
    if (Device)
    {
        RefreshBuiltInShader(InType);
    }
    return &Shaders[Idx];
}

FShader* FShaderManager::GetCustomShader(const FString& Key)
{
    const FString NormalizedKey = FPaths::FromPath(FPaths::ToPath(FPaths::ToWide(Key)).lexically_normal());
    auto It = CustomShaderCache.find(NormalizedKey);
    if (It != CustomShaderCache.end())
    {
        auto& Dependency = It->second.SourceFile;
        if (!ShaderDependencyUtils::HasDependencyChanged(Dependency))
        {
            return It->second.Shader.get();
        }
    }
    return nullptr;
}

FShader* FShaderManager::CreateCustomShader(ID3D11Device* InDevice, const wchar_t* InFilePath)
{
    Device = InDevice ? InDevice : Device;
    const FString Key = ShaderDependencyUtils::WStringToString(std::wstring(InFilePath));
    const FString NormalizedKey = FPaths::FromPath(FPaths::ToPath(FPaths::ToWide(Key)).lexically_normal());

    auto It = CustomShaderCache.find(NormalizedKey);
    if (It != CustomShaderCache.end())
    {
        auto& Dependency = It->second.SourceFile;
        if (!ShaderDependencyUtils::HasDependencyChanged(Dependency))
        {
            return It->second.Shader.get();
        }

        if (It->second.Shader)
        {
            RetiredCustomShaders.push_back(std::move(It->second.Shader));
        }
        CustomShaderCache.erase(It);
    }

    auto NewShader = std::make_unique<FShader>();
    NewShader->Create(Device, FPaths::ToWide(NormalizedKey).c_str(), "VS", "PS");
    auto* RawPtr = NewShader.get();

    FCustomShaderCacheEntry Entry;
    Entry.SourceFile = ShaderDependencyUtils::BuildFileDependency(NormalizedKey);
    Entry.Shader = std::move(NewShader);
    CustomShaderCache[NormalizedKey] = std::move(Entry);
    return RawPtr;
}

void FShaderManager::RefreshBuiltInShader(EShaderType InType)
{
    const uint32 Idx = (uint32)InType;
    if (Idx >= (uint32)EShaderType::MAX || !Device)
    {
        return;
    }

    const FString ShaderPath = GetBuiltInShaderPath(InType);
    if (ShaderPath.empty())
    {
        return;
    }

    auto& Dependency = BuiltInShaderFiles[Idx];
    if (Dependency.bExists && !ShaderDependencyUtils::HasDependencyChanged(Dependency))
    {
        return;
    }

    Shaders[Idx].Create(Device, FPaths::ToWide(ShaderPath).c_str(), "VS", "PS");
    Dependency = ShaderDependencyUtils::BuildFileDependency(ShaderPath);
}

FString FShaderManager::GetBuiltInShaderPath(EShaderType InType) const
{
    switch (InType)
    {
    case EShaderType::Primitive: return "Shaders/Primitive.hlsl";
    case EShaderType::Gizmo: return "Shaders/Gizmo.hlsl";
    case EShaderType::Editor: return "Shaders/Editor.hlsl";
    case EShaderType::StaticMesh: return "Shaders/StaticMeshShader.hlsl";
    case EShaderType::Decal: return "Shaders/DecalShader.hlsl";
    case EShaderType::OutlinePostProcess: return "Shaders/OutlinePostProcess.hlsl";
    case EShaderType::SceneDepth: return "Shaders/SceneDepth.hlsl";
    case EShaderType::NormalView: return "Shaders/NormalView.hlsl";
    case EShaderType::FXAA: return "Shaders/FXAA.hlsl";
    case EShaderType::Font: return "Shaders/ShaderFont.hlsl";
    case EShaderType::OverlayFont: return "Shaders/ShaderOverlayFont.hlsl";
    case EShaderType::SubUV: return "Shaders/ShaderSubUV.hlsl";
    case EShaderType::Billboard: return "Shaders/ShaderBillboard.hlsl";
    case EShaderType::HeightFog: return "Shaders/HeightFog.hlsl";
    case EShaderType::DepthOnly: return "Shaders/DepthOnly.hlsl";
    default: return "";
    }
}
