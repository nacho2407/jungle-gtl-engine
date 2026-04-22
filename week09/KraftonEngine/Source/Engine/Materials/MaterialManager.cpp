#include "Render/Passes/Base/RenderStateStrings.h"
#include "Render/Passes/Base/PipelineStateTypes.h"
#include "Render/Passes/Base/RenderPassTypes.h"
#include "MaterialManager.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_set>
#include "Materials/Material.h"
#include "Materials/MaterialSemantics.h"
#include "Platform/Paths.h"
#include "Render/Resources/Shaders/ShaderManager.h"
#include "Render/RHI/D3D11/Buffers/Buffers.h"
#include "Texture/Texture2D.h"
#include "Render/Renderer.h"

namespace MatKeys
{
static constexpr const char* PathFileName = "PathFileName";
static constexpr const char* ShaderPath = "ShaderPath";
static constexpr const char* RenderPass = "RenderPass";
static constexpr const char* BlendState = "BlendState";
static constexpr const char* DepthStencilState = "DepthStencilState";
static constexpr const char* RasterizerState = "RasterizerState";
static constexpr const char* Parameters = "Parameters";
static constexpr const char* Textures = "Textures";
} // namespace MatKeys


namespace
{
FString CanonicalizeTextureSlotName(const FString& SlotName)
{   
    return MaterialSemantics::CanonicalizeTextureSlot(SlotName);
}

FString CanonicalizeParameterName(const FString& ParamName)
{
    return MaterialSemantics::CanonicalizeParameterName(ParamName);
}

bool IsRuntimeGeneratedMaterialParameter(const FString& ParamName)
{
    return ParamName == "MaterialParam" ||
           ParamName == "HasBaseTexture" ||
           ParamName == "HasNormalTexture" ||
           ParamName == "HasSpecularTexture" ||
           ParamName == "StaticMeshMaterialPadding";
}

uint64 HashString64(const std::string& Value)
{
    uint64 Hash = 1469598103934665603ull;
    for (unsigned char Ch : Value)
    {
        Hash ^= static_cast<uint64>(Ch);
        Hash *= 1099511628211ull;
    }
    return Hash;
}

void HashCombine64(uint64& Seed, uint64 Value)
{
    Seed ^= Value + 0x9e3779b97f4a7c15ull + (Seed << 6) + (Seed >> 2);
}

bool TryExtractIncludePath(const std::string& Line, std::string& OutInclude)
{
    const size_t IncludePos = Line.find("#include");
    if (IncludePos == std::string::npos)
    {
        return false;
    }

    const size_t OpenQuote = Line.find('"', IncludePos);
    if (OpenQuote == std::string::npos)
    {
        return false;
    }

    const size_t CloseQuote = Line.find('"', OpenQuote + 1);
    if (CloseQuote == std::string::npos || CloseQuote <= OpenQuote + 1)
    {
        return false;
    }

    OutInclude = Line.substr(OpenQuote + 1, CloseQuote - OpenQuote - 1);
    return true;
}

uint64 BuildDependencyHashRecursive(const std::filesystem::path& FilePath, std::unordered_set<std::wstring>& Visited)
{
    std::error_code Ec;
    std::filesystem::path Canonical = std::filesystem::weakly_canonical(FilePath, Ec);
    if (Ec)
    {
        Canonical = FilePath.lexically_normal();
    }

    const std::wstring CanonicalKey = Canonical.generic_wstring();
    if (!Visited.insert(CanonicalKey).second)
    {
        return 0;
    }

    uint64 Hash = HashString64(std::string(CanonicalKey.begin(), CanonicalKey.end()));
    const bool bExists = std::filesystem::exists(Canonical, Ec) && !Ec;
    HashCombine64(Hash, bExists ? 1ull : 0ull);
    if (!bExists)
    {
        return Hash;
    }

    const auto LastWrite = std::filesystem::last_write_time(Canonical, Ec);
    if (!Ec)
    {
        HashCombine64(Hash, static_cast<uint64>(LastWrite.time_since_epoch().count()));
    }

    std::ifstream File(Canonical);
    if (!File.is_open())
    {
        return Hash;
    }

    std::string Line;
    while (std::getline(File, Line))
    {
        std::string IncludePath;
        if (!TryExtractIncludePath(Line, IncludePath))
        {
            continue;
        }

        std::filesystem::path IncludedFile = (Canonical.parent_path() / std::filesystem::path(IncludePath)).lexically_normal();
        HashCombine64(Hash, BuildDependencyHashRecursive(IncludedFile, Visited));
    }

    return Hash;
}

uint64 BuildDependencyHash(const std::filesystem::path& FilePath)
{
    std::unordered_set<std::wstring> Visited;
    return BuildDependencyHashRecursive(FilePath, Visited);
}
}

void FMaterialManager::ScanMaterialAssets()
{
    AvailableMaterialFiles.clear();

    const std::filesystem::path MaterialRoot = FPaths::RootDir() + L"Asset\\Materials\\";

    if (!std::filesystem::exists(MaterialRoot))
    {
        return;
    }

    const std::filesystem::path ProjectRoot(FPaths::RootDir());

    for (const auto& Entry : std::filesystem::recursive_directory_iterator(MaterialRoot))
    {
        if (!Entry.is_regular_file())
            continue;

        const std::filesystem::path& Path = Entry.path();

        if (Path.extension() != L".json")
            continue;
        if (Path.stem() == L"None")
            continue;

        FMaterialAssetListItem Item;
        Item.DisplayName = FPaths::ToUtf8(Path.stem().wstring());
        Item.FullPath = FPaths::ToUtf8(Path.lexically_relative(ProjectRoot).generic_wstring());
        AvailableMaterialFiles.push_back(std::move(Item));
    }
}

UMaterial* FMaterialManager::GetOrCreateStaticMeshMaterial(const FString& MatFilePath)
{
    return GetOrCreateMaterial(NormalizeCacheKey(MatFilePath));
}

UMaterial* FMaterialManager::GetOrCreateMaterial(const FString& MatFilePath)
{
    const FString CacheKey = NormalizeCacheKey(MatFilePath);

    auto It = MaterialCache.find(CacheKey);
    if (It != MaterialCache.end())
    {
        FMaterialCacheEntry& Cached = It->second;
        const bool bMaterialChanged = HasDependencyChanged(Cached.MaterialFile);
        const bool bShaderChanged = HasDependencyChanged(Cached.ShaderFile);
        const bool bTextureChanged = HasAnyDependencyChanged(Cached.TextureFiles);
        if (!bMaterialChanged && !bShaderChanged && !bTextureChanged)
        {
            return Cached.Material;
        }

        RetireMaterialCacheEntry(Cached);
        MaterialCache.erase(It);
    }

    json::JSON JsonData = ReadJsonFile(CacheKey);
    if (JsonData.IsNull())
    {
        UMaterial* DefaultMaterial = UObjectManager::Get().CreateObject<UMaterial>();
        FMaterialTemplate* Template = GetOrCreateTemplate(DefaultShaderPath);
        if (!Template)
        {
            UObjectManager::Get().DestroyObject(DefaultMaterial);
            return nullptr;
        }

        TMap<FString, std::unique_ptr<FMaterialConstantBuffer>> Buffers = CreateConstantBuffers(Template);
        DefaultMaterial->Create(CacheKey, Template, ERenderPass::Opaque, EBlendState::Opaque, EDepthStencilState::Default, ERasterizerState::SolidBackCull, std::move(Buffers));
        DefaultMaterial->SetVector4Parameter("SectionColor", FVector4(1.0f, 0.0f, 1.0f, 1.0f));

        FMaterialCacheEntry NewEntry;
        NewEntry.Material = DefaultMaterial;
        NewEntry.MaterialFile = BuildFileDependency(ResolveFullPath(CacheKey));
        NewEntry.ShaderFile = BuildFileDependency(ResolveFullPath(DefaultShaderPath));
        MaterialCache.emplace(CacheKey, std::move(NewEntry));
        return DefaultMaterial;
    }

    const bool bNormalized = NormalizeMaterialJson(JsonData, CacheKey);

    FString PathFileName = (JsonData.hasKey(MatKeys::PathFileName) && !JsonData[MatKeys::PathFileName].ToString().empty())
        ? JsonData[MatKeys::PathFileName].ToString().c_str()
        : CacheKey;
    FString ShaderPath = (JsonData.hasKey(MatKeys::ShaderPath) && !JsonData[MatKeys::ShaderPath].ToString().empty())
        ? JsonData[MatKeys::ShaderPath].ToString().c_str()
        : DefaultShaderPath;
    FString RenderPassStr = JsonData.hasKey(MatKeys::RenderPass) ? JsonData[MatKeys::RenderPass].ToString().c_str() : FString();
    ERenderPass RenderPass = StringToRenderPass(RenderPassStr);

    FString BlendStr = JsonData.hasKey(MatKeys::BlendState) ? JsonData[MatKeys::BlendState].ToString().c_str() : "";
    FString DepthStr = JsonData.hasKey(MatKeys::DepthStencilState) ? JsonData[MatKeys::DepthStencilState].ToString().c_str() : "";
    FString RasterStr = JsonData.hasKey(MatKeys::RasterizerState) ? JsonData[MatKeys::RasterizerState].ToString().c_str() : "";

    EBlendState BlendState = StringToBlendState(BlendStr, RenderPass);
    EDepthStencilState DepthState = StringToDepthStencilState(DepthStr, RenderPass);
    ERasterizerState RasterState = StringToRasterizerState(RasterStr, RenderPass);

    FMaterialTemplate* Template = GetOrCreateTemplate(ShaderPath);
    if (!Template)
        return nullptr;

    auto InjectedBuffers = CreateConstantBuffers(Template);

    UMaterial* Material = UObjectManager::Get().CreateObject<UMaterial>();
    Material->Create(PathFileName, Template, RenderPass, BlendState, DepthState, RasterState, std::move(InjectedBuffers));

    bool bInjected = InjectDefaultParameters(JsonData, Template, Material);
    bool bPurged = PurgeStaleParameters(JsonData, Template);

    ApplyParameters(Material, JsonData);
    ApplyTextures(Material, JsonData, CacheKey);

    if (bNormalized || bInjected || bPurged)
    {
        SaveToJSON(JsonData, CacheKey);
    }

    FMaterialCacheEntry NewEntry;
    NewEntry.Material = Material;
    NewEntry.MaterialFile = BuildFileDependency(ResolveFullPath(CacheKey));
    NewEntry.ShaderFile = BuildFileDependency(ResolveFullPath(ShaderPath));
    NewEntry.TextureFiles = CollectTextureDependencies(JsonData, CacheKey);
    MaterialCache.emplace(CacheKey, std::move(NewEntry));
    return Material;
}

json::JSON FMaterialManager::ReadJsonFile(const FString& FilePath) const
{
    const std::filesystem::path JsonPath = ResolveFullPath(FilePath);

    std::ifstream File(JsonPath);
    if (!File.is_open())
        return json::JSON();

    std::stringstream Buffer;
    Buffer << File.rdbuf();
    return json::JSON::Load(Buffer.str());
}

TMap<FString, std::unique_ptr<FMaterialConstantBuffer>> FMaterialManager::CreateConstantBuffers(FMaterialTemplate* Template)
{
    TMap<FString, std::unique_ptr<FMaterialConstantBuffer>> InjectedBuffers;

    const auto& RequiredBuffers = Template->GetParameterInfo();
    std::vector<FString> CreatedBuffers;

    for (const auto& BufferInfo : RequiredBuffers)
    {
        const FMaterialParameterInfo* ParamInfo = BufferInfo.second;

        if (std::find(CreatedBuffers.begin(), CreatedBuffers.end(), ParamInfo->BufferName) != CreatedBuffers.end())
            continue;

        auto MatCB = std::make_unique<FMaterialConstantBuffer>();
        MatCB->Init(Device, ParamInfo->BufferSize, ParamInfo->SlotIndex);

        InjectedBuffers.emplace(ParamInfo->BufferName, std::move(MatCB));
        CreatedBuffers.push_back(ParamInfo->BufferName);
    }

    return InjectedBuffers;
}

void FMaterialManager::ApplyParameters(UMaterial* Material, json::JSON& JsonData)
{
    if (!JsonData.hasKey(MatKeys::Parameters))
        return;

    for (auto& Pair : JsonData[MatKeys::Parameters].ObjectRange())
    {
        const FString ParamName = CanonicalizeParameterName(Pair.first.c_str());
        json::JSON& Value = Pair.second;

        if (Value.JSONType() == json::JSON::Class::Array)
        {
            if (Value.length() == 3)
            {
                Material->SetVector3Parameter(ParamName, FVector(Value[0].ToFloat(), Value[1].ToFloat(), Value[2].ToFloat()));
            }
            else if (Value.length() == 4)
            {
                Material->SetVector4Parameter(ParamName, FVector4(Value[0].ToFloat(), Value[1].ToFloat(), Value[2].ToFloat(), Value[3].ToFloat()));
            }
        }
        else if (Value.JSONType() == json::JSON::Class::Floating || Value.JSONType() == json::JSON::Class::Integral)
        {
            Material->SetScalarParameter(ParamName, Value.ToFloat());
        }
    }
}

void FMaterialManager::ApplyTextures(UMaterial* Material, json::JSON& JsonData, const FString& MatFilePath)
{
    if (!JsonData.hasKey(MatKeys::Textures))
        return;

    for (auto& Pair : JsonData[MatKeys::Textures].ObjectRange())
    {
        const FString SlotName = CanonicalizeTextureSlotName(Pair.first.c_str());
        const FString TexturePath = Pair.second.ToString().c_str();
        const std::filesystem::path ResolvedTexturePath = ResolveTexturePath(TexturePath, MatFilePath);

        UTexture2D* Texture = UTexture2D::LoadFromFile(FPaths::FromPath(ResolvedTexturePath), Device);
        if (Texture)
        {
            Material->SetTextureParameter(SlotName, Texture);
        }
    }
}

ERenderPass FMaterialManager::StringToRenderPass(const FString& Str) const
{
    using namespace RenderStateStrings;
    return FromString(RenderPassMap, Str, ERenderPass::Opaque);
}

EBlendState FMaterialManager::StringToBlendState(const FString& Str, ERenderPass Pass) const
{
    using namespace RenderStateStrings;
    if (!Str.empty())
        return FromString(BlendStateMap, Str, EBlendState::Opaque);

    switch (Pass)
    {
    case ERenderPass::AlphaBlend:
    case ERenderPass::Decal:
    case ERenderPass::EditorLines:
    case ERenderPass::PostProcess:
    case ERenderPass::GizmoInner:
    case ERenderPass::OverlayBillboard:
    case ERenderPass::OverlayTextWorld:
    case ERenderPass::OverlayFont:
        return EBlendState::AlphaBlend;
    case ERenderPass::AdditiveDecal:
        return EBlendState::Additive;
    case ERenderPass::SelectionMask:
        return EBlendState::NoColor;
    default:
        return EBlendState::Opaque;
    }
}

EDepthStencilState FMaterialManager::StringToDepthStencilState(const FString& Str, ERenderPass Pass) const
{
    using namespace RenderStateStrings;
    if (!Str.empty())
        return FromString(DepthStencilStateMap, Str, EDepthStencilState::Default);

    switch (Pass)
    {
    case ERenderPass::Decal:
    case ERenderPass::AdditiveDecal:
        return EDepthStencilState::DepthReadOnly;
    case ERenderPass::SelectionMask:
        return EDepthStencilState::StencilWrite;
    case ERenderPass::PostProcess:
    case ERenderPass::OverlayBillboard:
    case ERenderPass::OverlayTextWorld:
    case ERenderPass::OverlayFont:
        return EDepthStencilState::NoDepth;
    case ERenderPass::GizmoOuter:
        return EDepthStencilState::GizmoOutside;
    case ERenderPass::GizmoInner:
        return EDepthStencilState::GizmoInside;
    default:
        return EDepthStencilState::Default;
    }
}

ERasterizerState FMaterialManager::StringToRasterizerState(const FString& Str, ERenderPass Pass) const
{
    using namespace RenderStateStrings;
    if (!Str.empty())
        return FromString(RasterizerStateMap, Str, ERasterizerState::SolidBackCull);

    switch (Pass)
    {
    case ERenderPass::Decal:
    case ERenderPass::AdditiveDecal:
    case ERenderPass::SelectionMask:
    case ERenderPass::PostProcess:
    case ERenderPass::OverlayBillboard:
    case ERenderPass::OverlayTextWorld:
        return ERasterizerState::SolidNoCull;
    default:
        return ERasterizerState::SolidBackCull;
    }
}

bool FMaterialManager::NormalizeMaterialJson(json::JSON& JsonData, const FString& MaterialPath)
{
    bool bChanged = false;

    if (!JsonData.hasKey(MatKeys::PathFileName) || JsonData[MatKeys::PathFileName].ToString().empty())
    {
        JsonData[MatKeys::PathFileName] = MaterialPath.c_str();
        bChanged = true;
    }

    if (JsonData.hasKey(MatKeys::Textures))
    {
        json::JSON CanonicalTextures = json::JSON::Make(json::JSON::Class::Object);
        for (auto& Pair : JsonData[MatKeys::Textures].ObjectRange())
        {
            const FString CanonicalSlot = CanonicalizeTextureSlotName(Pair.first.c_str());
            CanonicalTextures[CanonicalSlot] = Pair.second;
            if (CanonicalSlot != Pair.first.c_str())
            {
                bChanged = true;
            }
        }
        JsonData[MatKeys::Textures] = std::move(CanonicalTextures);
    }

    if (JsonData.hasKey(MatKeys::Parameters))
    {
        json::JSON CanonicalParams = json::JSON::Make(json::JSON::Class::Object);
        for (auto& Pair : JsonData[MatKeys::Parameters].ObjectRange())
        {
            const FString CanonicalName = CanonicalizeParameterName(Pair.first.c_str());
            CanonicalParams[CanonicalName] = Pair.second;
            if (CanonicalName != Pair.first.c_str())
            {
                bChanged = true;
            }
        }
        JsonData[MatKeys::Parameters] = std::move(CanonicalParams);
    }

    if (!JsonData.hasKey(MatKeys::RenderPass) || JsonData[MatKeys::RenderPass].ToString().empty())
    {
        JsonData[MatKeys::RenderPass] = "Opaque";
        bChanged = true;
    }

    return bChanged;
}

void FMaterialManager::SaveToJSON(json::JSON& JsonData, const FString& MatFilePath)
{
    const std::filesystem::path FullPath = ResolveFullPath(MatFilePath);
    std::ofstream File(FullPath);
    File << JsonData.dump();
}

bool FMaterialManager::InjectDefaultParameters(json::JSON& JsonData, FMaterialTemplate* Template, UMaterial* Material)
{
    const auto& Layout = Template->GetParameterInfo();
    bool bInjected = false;

    for (const auto& Pair : Layout)
    {
        const FString& ParamName = Pair.first;
        const FMaterialParameterInfo* Info = Pair.second;

        if (IsRuntimeGeneratedMaterialParameter(ParamName))
            continue;

        if (!JsonData[MatKeys::Parameters][ParamName].IsNull())
            continue;

        bInjected = true;

        switch (Info->Size)
        {
        case sizeof(float):
        {
            float Value = 0.f;
            Material->GetScalarParameter(ParamName, Value);
            JsonData[MatKeys::Parameters][ParamName] = Value;
            break;
        }
        case sizeof(float) * 3:
        {
            FVector Value;
            Material->GetVector3Parameter(ParamName, Value);
            JsonData[MatKeys::Parameters][ParamName] = json::Array(Value.X, Value.Y, Value.Z);
            break;
        }
        case sizeof(float) * 4:
        {
            FVector4 Value;
            Material->GetVector4Parameter(ParamName, Value);
            JsonData[MatKeys::Parameters][ParamName] = json::Array(Value.X, Value.Y, Value.Z, Value.W);
            break;
        }
        case sizeof(float) * 16:
        {
            FMatrix Value;
            Material->GetMatrixParameter(ParamName, Value);
            auto MatArray = json::Array();
            for (int i = 0; i < 16; ++i)
                MatArray.append(Value.Data[i]);
            JsonData[MatKeys::Parameters][ParamName] = MatArray;
            break;
        }
        default:
            break;
        }
    }

    return bInjected;
}

bool FMaterialManager::PurgeStaleParameters(json::JSON& JsonData, FMaterialTemplate* Template)
{
    (void)JsonData;
    (void)Template;
    return false;
}

FMaterialTemplate* FMaterialManager::GetOrCreateTemplate(const FString& ShaderPath)
{
    const FString CacheKey = NormalizeCacheKey(ShaderPath);
    const FMaterialFileDependency CurrentShaderFile = BuildFileDependency(ResolveFullPath(CacheKey));

    auto It = TemplateCache.find(CacheKey);
    if (It != TemplateCache.end())
    {
        if (!HasDependencyChanged(It->second.ShaderFile))
        {
            return It->second.Template;
        }

        if (It->second.Template)
        {
            RetiredTemplates.push_back(It->second.Template);
        }
        TemplateCache.erase(It);
    }

    FShader* Shader = FShaderManager::Get().CreateCustomShader(Device, FPaths::ToWide(CacheKey).c_str());
    if (!Shader)
    {
        // Fallback: Shaders/StaticMeshShader.hlsl -> Shaders/Materials/StaticMeshShader.hlsl
        std::filesystem::path Path(FPaths::ToWide(ShaderPath));
        if (Path.parent_path() == L"Shaders")
        {
            FString FallbackPath = "Shaders/Materials/" + FPaths::FromWide(Path.filename().wstring());
            Shader = FShaderManager::Get().CreateCustomShader(Device, FPaths::ToWide(NormalizeCacheKey(FallbackPath)).c_str());
        }
        
        if (!Shader)
        {
            return nullptr;
        }
    }

    FMaterialTemplate* NewTemplate = new FMaterialTemplate();
    NewTemplate->Create(Shader);

    FTemplateCacheEntry Entry;
    Entry.Template = NewTemplate;
    Entry.ShaderFile = CurrentShaderFile;
    TemplateCache.emplace(CacheKey, Entry);
    return NewTemplate;
}

std::filesystem::path FMaterialManager::ResolveFullPath(const FString& FilePath) const
{
    std::filesystem::path Path = FPaths::ToPath(FPaths::ToWide(FilePath));
    if (!Path.is_absolute())
    {
        Path = FPaths::ToPath(FPaths::RootDir()) / Path;
    }

    std::error_code Ec;
    std::filesystem::path Canonical = std::filesystem::weakly_canonical(Path, Ec);
    if (!Ec)
    {
        return Canonical;
    }

    return Path.lexically_normal();
}

FString FMaterialManager::NormalizeCacheKey(const FString& FilePath) const
{
    return FPaths::FromPath(ResolveFullPath(FilePath));
}

FMaterialFileDependency FMaterialManager::BuildFileDependency(const std::filesystem::path& FilePath) const
{
    FMaterialFileDependency Dependency;
    Dependency.FullPath = FPaths::FromPath(FilePath);

    std::error_code Ec;
    Dependency.bExists = std::filesystem::exists(FilePath, Ec) && !Ec;
    if (Dependency.bExists)
    {
        Dependency.LastWriteTime = std::filesystem::last_write_time(FilePath, Ec);
        if (Ec)
        {
            Dependency.bExists = false;
            Dependency.LastWriteTime = {};
        }
    }

    Dependency.DependencyHash = BuildDependencyHash(FilePath);
    return Dependency;
}

bool FMaterialManager::HasDependencyChanged(const FMaterialFileDependency& Dependency) const
{
    if (Dependency.FullPath.empty())
    {
        return false;
    }

    const FMaterialFileDependency Current = BuildFileDependency(ResolveFullPath(Dependency.FullPath));
    if (Current.bExists != Dependency.bExists)
    {
        return true;
    }

    if (!Current.bExists)
    {
        return false;
    }

    return Current.LastWriteTime != Dependency.LastWriteTime || Current.DependencyHash != Dependency.DependencyHash;
}

bool FMaterialManager::HasAnyDependencyChanged(const std::vector<FMaterialFileDependency>& Dependencies) const
{
    for (const FMaterialFileDependency& Dependency : Dependencies)
    {
        if (HasDependencyChanged(Dependency))
        {
            return true;
        }
    }
    return false;
}

std::filesystem::path FMaterialManager::ResolveTexturePath(const FString& TexturePath, const FString& MatFilePath) const
{
    const std::filesystem::path MaterialPath = ResolveFullPath(MatFilePath);
    std::filesystem::path ResolvedTexturePath = FPaths::ToPath(FPaths::ToWide(TexturePath));
    if (!ResolvedTexturePath.is_absolute())
    {
        const std::filesystem::path RelativeToMaterial = (MaterialPath.parent_path() / ResolvedTexturePath).lexically_normal();
        const std::filesystem::path RelativeToRoot = (FPaths::ToPath(FPaths::RootDir()) / ResolvedTexturePath).lexically_normal();

        if (std::filesystem::exists(RelativeToMaterial))
        {
            ResolvedTexturePath = RelativeToMaterial;
        }
        else
        {
            ResolvedTexturePath = RelativeToRoot;
        }
    }

    std::error_code Ec;
    std::filesystem::path Canonical = std::filesystem::weakly_canonical(ResolvedTexturePath, Ec);
    if (!Ec)
    {
        return Canonical;
    }

    return ResolvedTexturePath.lexically_normal();
}

std::vector<FMaterialFileDependency> FMaterialManager::CollectTextureDependencies(json::JSON& JsonData, const FString& MatFilePath) const
{
    std::vector<FMaterialFileDependency> Result;
    if (!JsonData.hasKey(MatKeys::Textures))
    {
        return Result;
    }

    for (auto& Pair : JsonData[MatKeys::Textures].ObjectRange())
    {
        const FString TexturePath = Pair.second.ToString().c_str();
        Result.push_back(BuildFileDependency(ResolveTexturePath(TexturePath, MatFilePath)));
    }

    return Result;
}

void FMaterialManager::RetireMaterialCacheEntry(FMaterialCacheEntry& Entry)
{
    if (Entry.Material)
    {
        RetiredMaterials.push_back(Entry.Material);
        Entry.Material = nullptr;
    }
}

FMaterialManager::~FMaterialManager()
{
    if (!Device)
    {
        Release();
    }
}

void FMaterialManager::Release()
{
    for (auto& Pair : TemplateCache)
    {
        if (Pair.second.Template != nullptr)
        {
            delete Pair.second.Template;
            Pair.second.Template = nullptr;
        }
    }
    TemplateCache.clear();

    for (FMaterialTemplate* RetiredTemplate : RetiredTemplates)
    {
        delete RetiredTemplate;
    }
    RetiredTemplates.clear();

    for (auto& Pair : MaterialCache)
    {
        if (Pair.second.Material != nullptr)
        {
            UObjectManager::Get().DestroyObject(Pair.second.Material);
            Pair.second.Material = nullptr;
        }
    }
    MaterialCache.clear();

    for (UMaterial* RetiredMaterial : RetiredMaterials)
    {
        UObjectManager::Get().DestroyObject(RetiredMaterial);
    }
    RetiredMaterials.clear();

    Device = nullptr;
}
