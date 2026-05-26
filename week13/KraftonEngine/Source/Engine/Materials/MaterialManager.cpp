#include "MaterialManager.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include "Materials/Material.h"
#include "Materials/Graph/MaterialGraphAsset.h"
#include "Materials/Graph/MaterialGraphCompiler.h"
#include "Platform/Paths.h"
#include "Render/Shader/ShaderManager.h"
#include "Render/Resource/Buffer.h"
#include "Texture/Texture2D.h"
#include "Render/Pipeline/Renderer.h"

namespace
{
    float JsonNumberToFloat(const json::JSON& Value, float Default = 0.0f)
    {
        if (Value.JSONType() == json::JSON::Class::Floating) return static_cast<float>(Value.ToFloat());
        if (Value.JSONType() == json::JSON::Class::Integral) return static_cast<float>(Value.ToInt());
        return Default;
    }

    int32 JsonNumberToInt(const json::JSON& Value, int32 Default = 0)
    {
        if (Value.JSONType() == json::JSON::Class::Integral) return static_cast<int32>(Value.ToInt());
        if (Value.JSONType() == json::JSON::Class::Floating) return static_cast<int32>(Value.ToFloat());
        return Default;
    }

    FString NormalizeMaterialPath(const FString& Path)
    {
        std::filesystem::path P(FPaths::ToWide(Path));
        return FPaths::ToUtf8(P.generic_wstring());
    }

    bool ProjectFileExists(const FString& RelativePath)
    {
        std::filesystem::path Full(FPaths::ToWide(RelativePath));
        if (!Full.is_absolute())
        {
            Full = std::filesystem::path(FPaths::RootDir()) / Full;
        }
        return std::filesystem::exists(Full);
    }
    constexpr const char* MaterialGraphGeneratorVersion = "ParticleMeshLighting_v3";
}

void FMaterialManager::ScanMaterialAssets()
{
	AvailableMaterialFiles.clear();

	const std::filesystem::path MaterialRoot = FPaths::RootDir() + L"Content/Material/";

	if (!std::filesystem::exists(MaterialRoot))
	{
		return;
	}

	const std::filesystem::path ProjectRoot(FPaths::RootDir());

	for (const auto& Entry : std::filesystem::recursive_directory_iterator(MaterialRoot))
	{
		if (!Entry.is_regular_file()) continue;

		const std::filesystem::path& Path = Entry.path();

		if (Path.extension() != L".mat") continue;
		if (Path.stem() == L"None") continue; // Fallback 머티리얼은 목록에서 제외

		FMaterialAssetListItem Item;
		Item.DisplayName = FPaths::ToUtf8(Path.stem().wstring());
		Item.FullPath = FPaths::ToUtf8(Path.lexically_relative(ProjectRoot).generic_wstring());
		AvailableMaterialFiles.push_back(std::move(Item));
	}
}

UMaterial* FMaterialManager::GetOrCreateMaterial(const FString& MatFilePath)
{
	std::filesystem::path Path(FPaths::ToWide(MatFilePath));
	FString GenericPath = FPaths::ToUtf8(Path.generic_wstring());
	// 1. 캐시 반환
	auto It = MaterialCache.find(GenericPath);
	if (It != MaterialCache.end())
	{
		return It->second;
	}

    // 2. 캐시에 없다면 JSON에서 읽기
	json::JSON JsonData = ReadJsonFile(GenericPath);
	if (JsonData.IsNull())
	{
		// 기본 머티리얼 생성
		UMaterial* DefaultMaterial = UObjectManager::Get().CreateObject<UMaterial>();
		FMaterialTemplate* Template = GetOrCreateTemplate(DefaultShaderPath);
		TMap<FString, std::unique_ptr<FMaterialConstantBuffer>> Buffers = CreateConstantBuffers(Template);
		DefaultMaterial->Create(GenericPath, Template, ERenderPass::Opaque, EBlendState::Opaque, EDepthStencilState::Default, ERasterizerState::SolidBackCull, std::move(Buffers));
		// 폴백: 핑크색으로 미지정 머티리얼임을 표시
		DefaultMaterial->SetVector4Parameter("SectionColor", FVector4(1.0f, 0.0f, 1.0f, 1.0f));
		MaterialCache.emplace(GenericPath, DefaultMaterial);
		return DefaultMaterial;
	}

    UMaterial* Material = nullptr;
    if (!LoadMaterialFromJson(GenericPath, JsonData, nullptr, Material))
    {
        return nullptr;
    }

    MaterialCache.emplace(GenericPath, Material);
    return Material;
}

UMaterial* FMaterialManager::ReloadMaterial(const FString& MatFilePath)
{
    const FString GenericPath = NormalizeMaterialPath(MatFilePath);
    json::JSON    JsonData    = ReadJsonFile(GenericPath);
    if (JsonData.IsNull())
    {
        return nullptr;
    }

    UMaterial* Existing = nullptr;
    auto       It       = MaterialCache.find(GenericPath);
    if (It != MaterialCache.end())
    {
        Existing = It->second;
        if (Existing)
        {
            Existing->ResetRuntimeData();
        }
    }

    UMaterial* Material = nullptr;
    if (!LoadMaterialFromJson(GenericPath, JsonData, Existing, Material))
    {
        return nullptr;
    }

    MaterialCache[GenericPath] = Material;
    return Material;
}

void FMaterialManager::InvalidateMaterial(const FString& MatFilePath)
{
    MaterialCache.erase(NormalizeMaterialPath(MatFilePath));
}

json::JSON FMaterialManager::ReadJsonFile(const FString& FilePath) const
{
    std::ifstream File(FPaths::ToWide(FilePath).c_str());
    if (!File.is_open()) return json::JSON(); // Null JSON 반환

    std::stringstream Buffer;
    Buffer << File.rdbuf();
    return json::JSON::Load(Buffer.str());
}

bool FMaterialManager::LoadMaterialFromJson(
    const FString& MatFilePath,
    json::JSON&    JsonData,
    UMaterial*     ReuseMaterial,
    UMaterial*&    OutMaterial
    )
{
    EnsureGraphMaterialJsonDefaults(MatFilePath, JsonData);

    const bool bGraphMaterial = JsonData.hasKey(MatKeys::Graph);
    if (bGraphMaterial)
    {
        FString CompileError;
        if (!CompileMaterialGraph(MatFilePath, JsonData, &CompileError))
        {
            return false;
        }
    }

    FString PathFileName = JsonData.hasKey(MatKeys::PathFileName) ? JsonData[MatKeys::PathFileName].ToString().c_str()
    : MatFilePath;
    FString ShaderPath = JsonData.hasKey(MatKeys::GeneratedShaderPath) && !JsonData[MatKeys::GeneratedShaderPath].
                                                                           ToString().empty()
    ? JsonData[MatKeys::GeneratedShaderPath].ToString().c_str() : JsonData[MatKeys::ShaderPath].ToString().c_str();
    if (ShaderPath.empty())
    {
        ShaderPath = DefaultShaderPath;
    }

    const EMaterialDomain Domain = StringToDomain(
        JsonData.hasKey(MatKeys::Domain) ? JsonData[MatKeys::Domain].ToString().c_str() : ""
    );
    const FString RenderPassStr = JsonData.hasKey(MatKeys::RenderPass)
    ? JsonData[MatKeys::RenderPass].ToString().c_str() : "";
	ERenderPass RenderPass = StringToRenderPass(RenderPassStr);

	FString BlendStr = JsonData.hasKey(MatKeys::BlendState) ? JsonData[MatKeys::BlendState].ToString().c_str() : "";
	FString DepthStr = JsonData.hasKey(MatKeys::DepthStencilState) ? JsonData[MatKeys::DepthStencilState].ToString().c_str() : "";
	FString RasterStr = JsonData.hasKey(MatKeys::RasterizerState) ? JsonData[MatKeys::RasterizerState].ToString().c_str() : "";

	EBlendState BlendState = StringToBlendState(BlendStr, RenderPass);
	EDepthStencilState DepthState = StringToDepthStencilState(DepthStr, RenderPass);
	ERasterizerState RasterState = StringToRasterizerState(RasterStr, RenderPass);

    FShaderKey ShaderKey(ShaderPath);
    if (bGraphMaterial)
    {
        ShaderKey.SetVertexFactory(DomainToVertexFactory(Domain));
    }

    FMaterialTemplate* Template = bGraphMaterial ? GetOrCreateTemplate(ShaderKey) : GetOrCreateTemplate(ShaderPath);
    if (!Template)
    {
        return false;
    }

	auto InjectedBuffers = CreateConstantBuffers(Template);

    UMaterial* Material = ReuseMaterial ? ReuseMaterial : UObjectManager::Get().CreateObject<UMaterial>();

    // Create()는 PathFileName/Template/렌더상태/CB만 처리하고 메타데이터는 건드리지 않으므로,
    // Create 호출 이후에만 한 번 셋팅하면 충분.
	Material->Create(PathFileName, Template, RenderPass, BlendState, DepthState, RasterState, std::move(InjectedBuffers));
    Material->SetSourcePath(MatFilePath);
    Material->SetMaterialGuid(
        JsonData.hasKey(MatKeys::MaterialGuid) ? JsonData[MatKeys::MaterialGuid].ToString().c_str() : ""
    );
    Material->SetDomain(Domain);
    Material->SetGeneratedShaderPath(
        JsonData.hasKey(MatKeys::GeneratedShaderPath) ? JsonData[MatKeys::GeneratedShaderPath].ToString().c_str() : ""
    );
    Material->SetReceiveLighting(
        JsonData.hasKey(MatKeys::ReceiveLighting) && JsonData[MatKeys::ReceiveLighting].ToBool()
    );

    FMaterialGraph Graph;
    if (JsonData.hasKey(MatKeys::Graph))
    {
        MaterialGraphAsset::LoadFromJson(JsonData[MatKeys::Graph], Graph);
    }
    Material->SetGraph(Graph);

    bool bInjected = !bGraphMaterial && InjectDefaultParameters(JsonData, Template, Material);
    bool bPurged   = !bGraphMaterial && PurgeStaleParameters(JsonData, Template);

	ApplyParameters(Material, JsonData);
	ApplyTextures(Material, JsonData);
	Material->RebuildCachedSRVs();

	JsonData[MatKeys::BlendState] = BlendStr.empty() ? "" : BlendStr.c_str();
	JsonData[MatKeys::DepthStencilState] = DepthStr.empty() ? "" : DepthStr.c_str();
	JsonData[MatKeys::RasterizerState] = RasterStr.empty() ? "" : RasterStr.c_str();

	if (bInjected || bPurged)
	{
        SaveToJSON(JsonData, MatFilePath);
    }

    OutMaterial = Material;
    return true;
}

TMap<FString, std::unique_ptr<FMaterialConstantBuffer>> FMaterialManager::CreateConstantBuffers(FMaterialTemplate* Template)
{

	TMap<FString, std::unique_ptr<FMaterialConstantBuffer>> InjectedBuffers;
    if (!Template)
    {
        return InjectedBuffers;
    }

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
    json::JSON* ParamsJson = nullptr;
    if (JsonData.hasKey(MatKeys::Compiled) && JsonData[MatKeys::Compiled].hasKey(MatKeys::Parameters))
    {
        ParamsJson = &JsonData[MatKeys::Compiled][MatKeys::Parameters];
    }
    else if (JsonData.hasKey(MatKeys::Parameters))
    {
        ParamsJson = &JsonData[MatKeys::Parameters];
    }
    if (!ParamsJson) return;

    for (auto& Pair : ParamsJson->ObjectRange())
	{
		FString ParamName = Pair.first.c_str();
        // Object 형태({Type,Value})면 Value만 꺼내 로컬로 복사. 순회 중 원본 mutate 금지.
        json::JSON Value = (Pair.second.JSONType() == json::JSON::Class::Object && Pair.second.hasKey("Value"))
        ? Pair.second["Value"] : Pair.second;

		if (Value.JSONType() == json::JSON::Class::Array)
		{
			if (Value.length() == 3)
			{
                Material->SetVector3Parameter(
                    ParamName,
                    FVector(JsonNumberToFloat(Value[0]), JsonNumberToFloat(Value[1]), JsonNumberToFloat(Value[2]))
                );
			}
			else if (Value.length() == 4)
			{
                Material->SetVector4Parameter(
                    ParamName,
                    FVector4(
                        JsonNumberToFloat(Value[0]),
                        JsonNumberToFloat(Value[1]),
                        JsonNumberToFloat(Value[2]),
                        JsonNumberToFloat(Value[3])
                    )
                );
			}
		}
		else if (Value.JSONType() == json::JSON::Class::Floating || Value.JSONType() == json::JSON::Class::Integral)
		{
            Material->SetScalarParameter(ParamName, JsonNumberToFloat(Value));
		}
	}
}

void FMaterialManager::ApplyTextures(UMaterial* Material, json::JSON& JsonData)
{
    json::JSON* TexturesJson = nullptr;
    if (JsonData.hasKey(MatKeys::Compiled) && JsonData[MatKeys::Compiled].hasKey(MatKeys::Textures))
    {
        TexturesJson = &JsonData[MatKeys::Compiled][MatKeys::Textures];
    }
    else if (JsonData.hasKey(MatKeys::Textures))
    {
        TexturesJson = &JsonData[MatKeys::Textures];
    }
    if (!TexturesJson) return;

    for (auto& Pair : TexturesJson->ObjectRange())
	{
		FString SlotName = Pair.first.c_str();
        FString TexturePath;
        if (Pair.second.JSONType() == json::JSON::Class::Object)
        {
            TexturePath        = Pair.second.hasKey("Path") ? Pair.second["Path"].ToString().c_str() : "";
            const FString Slot = Pair.second.hasKey("Slot") ? Pair.second["Slot"].ToString().c_str() : SlotName;
            SlotName           = FString(
                ToString(MaterialTextureSlotFromString(Slot, EMaterialTextureSlot::Diffuse))
            ) + "Texture";
        }
        else
        {
            TexturePath = Pair.second.ToString().c_str();
        }
        if (TexturePath.empty())
        {
            continue;
        }
		const bool bIsColorTexture =
			SlotName == "DiffuseTexture" ||
			SlotName == "EmissiveTexture" ||
			SlotName == "Custom0Texture" ||
			SlotName == "Custom1Texture";

		UTexture2D* Texture = UTexture2D::LoadFromFile(
			TexturePath,
			Device,
			bIsColorTexture ? ETextureColorSpace::SRGB : ETextureColorSpace::Linear);
		if (Texture)
		{
			Material->SetTextureParameter(SlotName, Texture);
		}
	}
}

EMaterialDomain FMaterialManager::StringToDomain(const FString& Str) const
{
    return MaterialDomainFromString(Str, EMaterialDomain::Surface);
}

EShaderVertexFactory FMaterialManager::DomainToVertexFactory(EMaterialDomain Domain) const
{
    switch (Domain)
    {
    case EMaterialDomain::ParticleSprite:
        return EShaderVertexFactory::ParticleSprite;
    case EMaterialDomain::ParticleMesh:
        return EShaderVertexFactory::ParticleMesh;
    case EMaterialDomain::PostProcess:
        return EShaderVertexFactory::Fullscreen;
    case EMaterialDomain::Surface:
    case EMaterialDomain::Decal: default:
        return EShaderVertexFactory::StaticMesh;
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

	// 문자열이 비어있으면 Pass 기반 기본값
	switch (Pass)
	{
	case ERenderPass::AlphaBlend:
	case ERenderPass::Decal:
	case ERenderPass::EditorLines:
	case ERenderPass::PostProcess:
	case ERenderPass::GizmoInner:
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

	// 문자열이 비어있으면 Pass 기반 기본값
	switch (Pass)
	{
	case ERenderPass::AlphaBlend:
	case ERenderPass::Decal:
	case ERenderPass::AdditiveDecal:
		return EDepthStencilState::DepthReadOnly;
	case ERenderPass::SelectionMask:
		return EDepthStencilState::StencilWrite;
	case ERenderPass::PostProcess:
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

	// 문자열이 비어있으면 Pass 기반 기본값
	switch (Pass)
	{
	case ERenderPass::Decal:
	case ERenderPass::AdditiveDecal:
	case ERenderPass::SelectionMask:
	case ERenderPass::PostProcess:
		return ERasterizerState::SolidNoCull;
	default:
		return ERasterizerState::SolidBackCull;
	}
}

void FMaterialManager::SaveToJSON(json::JSON& JsonData, const FString& MatFilePath)
{
	std::ofstream File(FPaths::ToWide(MatFilePath));
	File << JsonData.dump();
}

bool FMaterialManager::SaveMaterialJson(const FString& MatFilePath, const json::JSON& JsonData)
{
    std::ofstream File(FPaths::ToWide(MatFilePath));
    if (!File.is_open())
    {
        return false;
    }
    File << JsonData.dump();
    return true;
}

bool FMaterialManager::SaveMaterialAsset(UMaterial* Material)
{
    if (!Material || Material->GetSourcePath().empty())
    {
        return false;
    }

    json::JSON JsonData = ReadJsonFile(Material->GetSourcePath());
    if (JsonData.IsNull())
    {
        JsonData = json::JSON::Make(json::JSON::Class::Object);
    }

    JsonData[MatKeys::Version]      = 2;
    JsonData[MatKeys::MaterialGuid] = Material->GetMaterialGuid();
    JsonData[MatKeys::PathFileName] = Material->GetAssetPathFileName();
    JsonData[MatKeys::Domain]       = ToString(Material->GetDomain());
    JsonData[MatKeys::RenderPass]   = RenderStateStrings::ToString(
        RenderStateStrings::RenderPassMap,
        Material->GetRenderPass()
    );
    JsonData[MatKeys::BlendState] = RenderStateStrings::ToString(
        RenderStateStrings::BlendStateMap,
        Material->GetBlendState()
    );
    JsonData[MatKeys::DepthStencilState] = RenderStateStrings::ToString(
        RenderStateStrings::DepthStencilStateMap,
        Material->GetDepthStencilState()
    );
    JsonData[MatKeys::RasterizerState] = RenderStateStrings::ToString(
        RenderStateStrings::RasterizerStateMap,
        Material->GetRasterizerState()
    );
    JsonData[MatKeys::GeneratedShaderPath] = Material->GetGeneratedShaderPath();
    JsonData[MatKeys::ReceiveLighting] = Material->GetReceiveLighting();

    json::JSON GraphJson;
    MaterialGraphAsset::SaveToJson(Material->GetGraph(), GraphJson);
    JsonData[MatKeys::Graph] = std::move(GraphJson);

    // 컴파일 + 저장이 모두 성공한 뒤에만 런타임 상태를 갈아끼움.
    // 실패 시 머티리얼은 기존 셰이더/CB를 유지해 렌더가 계속 가능.
    const FString SourcePath = Material->GetSourcePath();
    FString       CompileError;
    if (!CompileMaterialGraph(SourcePath, JsonData, &CompileError))
    {
        return false;
    }

    if (!SaveMaterialJson(SourcePath, JsonData))
    {
        return false;
    }

    Material->ResetRuntimeData();
    ReloadMaterial(SourcePath);
    return true;
}

bool FMaterialManager::CompileMaterialGraph(const FString& MatFilePath, json::JSON& InOutJson, FString* OutError)
{
    if (!InOutJson.hasKey(MatKeys::Graph))
    {
        return true;
    }

    FMaterialGraph Graph;
    if (!MaterialGraphAsset::LoadFromJson(InOutJson[MatKeys::Graph], Graph))
    {
        if (OutError) *OutError = "Failed to read material graph.";
        return false;
    }

    const FString GraphHash = MaterialGraphAsset::ComputeGraphHashString(InOutJson[MatKeys::Graph]);
    const FString OldHash   = InOutJson.hasKey(MatKeys::Compiled) && InOutJson[MatKeys::Compiled].
    hasKey(MatKeys::GraphHash) ? InOutJson[MatKeys::Compiled][MatKeys::GraphHash].ToString() : FString();
    const FString OldGeneratorVersion = InOutJson.hasKey(MatKeys::Compiled) && InOutJson[MatKeys::Compiled].
    hasKey(MatKeys::GeneratorVersion) ? InOutJson[MatKeys::Compiled][MatKeys::GeneratorVersion].ToString() : FString();
    const FString ExistingGeneratedPath = InOutJson.hasKey(MatKeys::GeneratedShaderPath)
    ? InOutJson[MatKeys::GeneratedShaderPath].ToString() : FString();

    // ReceiveLighting 상태를 Compiled 섹션에 저장해 변경 감지
    const bool bCurrentReceiveLighting = InOutJson.hasKey(MatKeys::ReceiveLighting) && InOutJson[MatKeys::ReceiveLighting].ToBool();
    const bool bCompiledReceiveLighting = InOutJson.hasKey(MatKeys::Compiled) && InOutJson[MatKeys::Compiled].hasKey(MatKeys::ReceiveLighting)
        && InOutJson[MatKeys::Compiled][MatKeys::ReceiveLighting].ToBool();

    // graph 미변경이어도 generator/runtime 코드가 바뀌면 강제 재컴파일.
    const bool bNeedsCompile =
        GraphHash != OldHash
        || OldGeneratorVersion != MaterialGraphGeneratorVersion
        || ExistingGeneratedPath.empty()
        || !ProjectFileExists(ExistingGeneratedPath)
        || bCurrentReceiveLighting != bCompiledReceiveLighting;
    if (!bNeedsCompile)
    {
        return true;
    }

    FMaterialCompileOptions Options;
    Options.MaterialPath = MatFilePath;
    Options.MaterialGuid = InOutJson.hasKey(MatKeys::MaterialGuid) ? InOutJson[MatKeys::MaterialGuid].ToString() : "";
    Options.Domain = StringToDomain(InOutJson.hasKey(MatKeys::Domain) ? InOutJson[MatKeys::Domain].ToString() : "");
    Options.RenderPass = StringToRenderPass(
        InOutJson.hasKey(MatKeys::RenderPass) ? InOutJson[MatKeys::RenderPass].ToString() : ""
    );
    Options.BlendState = StringToBlendState(
        InOutJson.hasKey(MatKeys::BlendState) ? InOutJson[MatKeys::BlendState].ToString() : "",
        Options.RenderPass
    );
    Options.DepthStencilState = StringToDepthStencilState(
        InOutJson.hasKey(MatKeys::DepthStencilState) ? InOutJson[MatKeys::DepthStencilState].ToString() : "",
        Options.RenderPass
    );
    Options.RasterizerState = StringToRasterizerState(
        InOutJson.hasKey(MatKeys::RasterizerState) ? InOutJson[MatKeys::RasterizerState].ToString() : "",
        Options.RenderPass
    );
    Options.bReceiveLighting = InOutJson.hasKey(MatKeys::ReceiveLighting) && InOutJson[MatKeys::ReceiveLighting].ToBool();

    FMaterialCompileResult Result;
    if (!FMaterialGraphCompiler::Compile(Graph, Options, Result))
    {
        if (OutError)
        {
            *OutError = Result.Errors.empty() ? FString("Material graph compile failed.") : Result.Errors.front();
        }
        return false;
    }

    const std::filesystem::path GeneratedPath = std::filesystem::path(FPaths::RootDir()) / FPaths::ToWide(
        Result.GeneratedShaderPath
    );
    std::filesystem::create_directories(GeneratedPath.parent_path());
    {
        std::ofstream File(GeneratedPath);
        if (!File.is_open())
        {
            if (OutError) *OutError = "Failed to write generated material shader.";
            return false;
        }
        File << Result.GeneratedHlsl;
    }

    InOutJson[MatKeys::GeneratedShaderPath]                  = Result.GeneratedShaderPath;
    InOutJson[MatKeys::ShaderPath]                           = Result.GeneratedShaderPath;
    InOutJson[MatKeys::Compiled]                                  = json::JSON::Make(json::JSON::Class::Object);
    InOutJson[MatKeys::Compiled][MatKeys::GraphHash]              = GraphHash;
    InOutJson[MatKeys::Compiled][MatKeys::GeneratorVersion]       = MaterialGraphGeneratorVersion;
    InOutJson[MatKeys::Compiled][MatKeys::ReceiveLighting]        = bCurrentReceiveLighting;
    InOutJson[MatKeys::Compiled][MatKeys::Parameters]             = json::JSON::Make(json::JSON::Class::Object);
    InOutJson[MatKeys::Compiled][MatKeys::Textures]               = json::JSON::Make(json::JSON::Class::Object);

    for (const auto& Pair : Result.Parameters)
    {
        const FString&                    Name = Pair.first;
        const FMaterialCompiledParameter& Param = Pair.second;
        json::JSON                        ParamJson = json::JSON::Make(json::JSON::Class::Object);
        ParamJson["Type"] = ToString(Param.Type);
        ParamJson["Value"] = json::Array(Param.Value.X, Param.Value.Y, Param.Value.Z, Param.Value.W);
        InOutJson[MatKeys::Compiled][MatKeys::Parameters][Name] = std::move(ParamJson);
    }

    for (const auto& Pair : Result.Textures)
    {
        const FString&                  Name                  = Pair.first;
        const FMaterialCompiledTexture& Texture               = Pair.second;
        json::JSON                      TexJson               = json::JSON::Make(json::JSON::Class::Object);
        TexJson["Path"]                                       = Texture.Path;
        TexJson["Slot"]                                       = ToString(Texture.Slot);
        InOutJson[MatKeys::Compiled][MatKeys::Textures][Name] = std::move(TexJson);
    }

    FShaderManager::Get().InvalidatePath(Result.GeneratedShaderPath);
    auto DropTemplate = [this](const FString& CacheKey)
    {
        auto It = TemplateCache.find(CacheKey);
        if (It == TemplateCache.end())
        {
            return;
        }
        delete It->second;
        TemplateCache.erase(It);
    };
    DropTemplate(Result.GeneratedShaderPath);
    DropTemplate(
        Result.GeneratedShaderPath + "#" + std::to_string(static_cast<int>(DomainToVertexFactory(Options.Domain)))
    );

    // 재컴파일 결과(GeneratedShaderPath, GeneratorVersion 등)를 .mat 파일에 저장.
    // 저장하지 않으면 다음 실행 시 버전 불일치로 인해 매번 재컴파일이 반복된다.
    SaveToJSON(InOutJson, MatFilePath);

    return true;
}

void FMaterialManager::EnsureGraphMaterialJsonDefaults(const FString& MatFilePath, json::JSON& JsonData)
{
    if (!JsonData.hasKey(MatKeys::Graph))
    {
        return;
    }

    if (!JsonData.hasKey(MatKeys::Version)) JsonData[MatKeys::Version] = 2;
    if (!JsonData.hasKey(MatKeys::MaterialGuid) || JsonData[MatKeys::MaterialGuid].ToString().empty())
    {
        char Buffer[32];
        std::snprintf(
            Buffer,
            sizeof(Buffer),
            "%016llX",
            static_cast<unsigned long long>(std::hash<FString> {}(MatFilePath))
        );
        JsonData[MatKeys::MaterialGuid] = Buffer;
    }
    if (!JsonData.hasKey(MatKeys::PathFileName)) JsonData[MatKeys::PathFileName] = MatFilePath;
    if (!JsonData.hasKey(MatKeys::Domain)) JsonData[MatKeys::Domain] = "ParticleSprite";
    if (!JsonData.hasKey(MatKeys::RenderPass)) JsonData[MatKeys::RenderPass] = "AlphaBlend";
    if (!JsonData.hasKey(MatKeys::BlendState)) JsonData[MatKeys::BlendState] = "AlphaBlend";
    if (!JsonData.hasKey(MatKeys::DepthStencilState)) JsonData[MatKeys::DepthStencilState] = "DepthReadOnly";
    if (!JsonData.hasKey(MatKeys::RasterizerState)) JsonData[MatKeys::RasterizerState] = "SolidNoCull";
    if (!JsonData.hasKey(MatKeys::GeneratedShaderPath)) JsonData[MatKeys::GeneratedShaderPath] = "";
    if (!JsonData.hasKey(MatKeys::Compiled))
    {
        JsonData[MatKeys::Compiled]                      = json::JSON::Make(json::JSON::Class::Object);
        JsonData[MatKeys::Compiled][MatKeys::Parameters] = json::JSON::Make(json::JSON::Class::Object);
        JsonData[MatKeys::Compiled][MatKeys::Textures]   = json::JSON::Make(json::JSON::Class::Object);
    }
}

bool FMaterialManager::InjectDefaultParameters(json::JSON& JsonData, FMaterialTemplate* Template, UMaterial* Material)
{
	const auto& Layout = Template->GetParameterInfo();
	bool bInjected = false;

	for (const auto& Pair : Layout)
	{
		const FString& ParamName = Pair.first;
		const FMaterialParameterInfo* Info = Pair.second;

		// 이미 JSON에 있으면 스킵
		if (!JsonData[MatKeys::Parameters][ParamName].IsNull())
			continue;

		bInjected = true;

		if (ParamName == "SectionColor")
		{
			JsonData[MatKeys::Parameters][ParamName] = json::Array(1.0f, 1.0f, 1.0f, 1.0f);
			continue;
		}

		if (ParamName == "HasNormalMap")
		{
			JsonData[MatKeys::Parameters][ParamName] = 0.0f;
			continue;
		}

		switch (Info->Size)
		{
			case sizeof(float) : // 4바이트 - Scalar
			{
				float Value = 0.f;
				Material->GetScalarParameter(ParamName, Value);
				JsonData[MatKeys::Parameters][ParamName] = Value;
				break;
			}
			case sizeof(float) * 3: // 12바이트 - Vector3
			{
				FVector Value;
				Material->GetVector3Parameter(ParamName, Value);
				JsonData[MatKeys::Parameters][ParamName] = json::Array(Value.X, Value.Y, Value.Z);
				break;
			}
			case sizeof(float) * 4: // 16바이트 - Vector4
			{
				FVector4 Value;
				Material->GetVector4Parameter(ParamName, Value);
				JsonData[MatKeys::Parameters][ParamName] = json::Array(Value.X, Value.Y, Value.Z, Value.W);
				break;
			}
			case sizeof(float) * 16: // 64바이트 - Matrix
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
				break; // uint, bool 등 특수 케이스는 별도 처리 필요
		}
	}

	return bInjected;
}

bool FMaterialManager::PurgeStaleParameters(json::JSON& JsonData, FMaterialTemplate* Template)
{
	if (!JsonData.hasKey(MatKeys::Parameters)) return false;

	const auto& Layout = Template->GetParameterInfo();
	json::JSON CleanParams = json::JSON::Make(json::JSON::Class::Object);
	bool bPurged = false;

	for (auto& Pair : JsonData[MatKeys::Parameters].ObjectRange())
	{
		FString ParamName = Pair.first.c_str();
		if (Layout.find(ParamName) != Layout.end())
		{
			CleanParams[Pair.first] = Pair.second;
		}
		else
		{
			bPurged = true;
		}
	}

	if (bPurged)
	{
		JsonData[MatKeys::Parameters] = std::move(CleanParams);
	}

	return bPurged;
}

FMaterialTemplate* FMaterialManager::GetOrCreateTemplate(const FString& ShaderPath)
{
	// 1. 템플릿이 캐시에 있는지 확인 (셰이더 경로를 키값으로 사용)
	auto It = TemplateCache.find(ShaderPath);
	if (It != TemplateCache.end())
	{
		return It->second;
	}

	// 2. 템플릿이 기존에 없다면 새로 제작
	//    캐시에 있으면 반환, 없으면 컴파일 후 캐싱
	FShader* Shader = FShaderManager::Get().FindOrCreate(ShaderPath);
	if (!Shader)
	{
		return nullptr;
	}

	FMaterialTemplate* NewTemplate = new FMaterialTemplate();
	NewTemplate->Create(Shader);
	TemplateCache.emplace(ShaderPath, NewTemplate);
	return NewTemplate;
}

FMaterialTemplate* FMaterialManager::GetOrCreateTemplate(const FShaderKey& Key)
{
    FString CacheKey = Key.Path + "#" + std::to_string(static_cast<int>(Key.VertexFactory));
    auto    It       = TemplateCache.find(CacheKey);
    if (It != TemplateCache.end())
    {
        return It->second;
    }

    FShader* Shader = FShaderManager::Get().FindOrCreate(Key);
    if (!Shader)
    {
        return nullptr;
    }

    FMaterialTemplate* NewTemplate = new FMaterialTemplate();
    NewTemplate->Create(Shader);
    TemplateCache.emplace(CacheKey, NewTemplate);
    return NewTemplate;
}

void FMaterialManager::Initialize(ID3D11Device* InDevice)
{
    Device = InDevice;

    if (Device && !FallbackWhiteSRV)
    {
        const uint32 WhitePixel = 0xFFFFFFFF;

        D3D11_TEXTURE2D_DESC Desc = {};
        Desc.Width                = 1;
        Desc.Height               = 1;
        Desc.MipLevels            = 1;
        Desc.ArraySize            = 1;
        Desc.Format               = DXGI_FORMAT_R8G8B8A8_UNORM;
        Desc.SampleDesc.Count     = 1;
        Desc.Usage                = D3D11_USAGE_IMMUTABLE;
        Desc.BindFlags            = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA Init = {};
        Init.pSysMem                = &WhitePixel;
        Init.SysMemPitch            = sizeof(WhitePixel);

        if (SUCCEEDED(Device->CreateTexture2D(&Desc, &Init, &FallbackWhiteTexture)) && FallbackWhiteTexture)
        {
            Device->CreateShaderResourceView(FallbackWhiteTexture, nullptr, &FallbackWhiteSRV);
        }
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
	// 1. TemplateCache 메모리 해제
	// GetOrCreateTemplate()에서 new FMaterialTemplate()로 직접 할당했으므로 여기서 delete 해줍니다.
	for (auto& Pair : TemplateCache)
	{
		if (Pair.second != nullptr)
		{
			delete Pair.second;
			Pair.second = nullptr;
		}
	}

	TemplateCache.clear();

	// 2. GPU 버퍼를 Device 해제 전에 명시 해제, UObject 수명은 UObjectManager가 관리
	for (auto& [Key, Mat] : MaterialCache)
	{
		if (Mat) Mat->ReleaseGPUBuffers();
	}
	MaterialCache.clear();

    // 3. Fallback 화이트 SRV/Texture 해제
    if (FallbackWhiteSRV)
    {
        FallbackWhiteSRV->Release();
        FallbackWhiteSRV = nullptr;
    }
    if (FallbackWhiteTexture)
    {
        FallbackWhiteTexture->Release();
        FallbackWhiteTexture = nullptr;
    }

    // 4. Device 참조 해제
	// 외부에서 주입받은 리소스이므로 포인터만 초기화합니다.
	Device = nullptr;
}
